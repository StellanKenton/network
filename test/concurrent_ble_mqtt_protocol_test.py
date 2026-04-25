from __future__ import annotations

import argparse
import asyncio
import sys
import time
from collections import deque
from pathlib import Path

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic

TEST_DIR = Path(__file__).resolve().parent
if str(TEST_DIR) not in sys.path:
    sys.path.insert(0, str(TEST_DIR))

from esp32c5_ble_tester import DEFAULT_DEVICE_NAME, DEFAULT_NOTIFY_UUID, DEFAULT_WRITE_UUID, resolve_characteristics
from local_iot_service import LocalIotService
from local_iot_workflow import RttClient, publish_with_retry


FRAME_HEAD = b"\xFA\xFC\x01"
CMD_HANDSHAKE = 0x01
CMD_HEARTBEAT = 0x03
CMD_DEV_INFO = 0x11


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(cmd: int, payload: bytes = b"") -> bytes:
    length = len(payload).to_bytes(2, "big")
    crc_data = bytes([cmd]) + length + payload
    crc = crc16_ccitt(crc_data).to_bytes(2, "big")
    return FRAME_HEAD + bytes([cmd]) + length + payload + crc


def pop_frames(buffer: bytearray) -> list[bytes]:
    frames: list[bytes] = []
    while True:
        head = buffer.find(FRAME_HEAD)
        if head < 0:
            buffer.clear()
            return frames
        if head > 0:
            del buffer[:head]
        if len(buffer) < 8:
            return frames
        payload_len = (buffer[4] << 8) | buffer[5]
        frame_len = 8 + payload_len
        if len(buffer) < frame_len:
            return frames
        frame = bytes(buffer[:frame_len])
        del buffer[:frame_len]
        crc_expected = (frame[-2] << 8) | frame[-1]
        if crc16_ccitt(frame[3:-2]) == crc_expected:
            frames.append(frame)


def frame_cmd(frame: bytes) -> int:
    return frame[3]


def mac_text_to_bytes(text: str) -> bytes:
    compact = "".join(ch for ch in text if ch.isalnum())
    if len(compact) != 12:
        raise ValueError("BLE MAC must contain 12 hex digits")
    return bytes.fromhex(compact)


class BleProtocolClient:
    def __init__(self, device_name: str, write_uuid: str | None, notify_uuid: str | None, timeout: float) -> None:
        self.device_name = device_name
        self.write_uuid = write_uuid
        self.notify_uuid = notify_uuid
        self.timeout = timeout
        self.client: BleakClient | None = None
        self.write_char: BleakGATTCharacteristic | None = None
        self.notify_char: BleakGATTCharacteristic | None = None
        self.rx_buffer = bytearray()
        self.frames: deque[bytes] = deque()
        self.event = asyncio.Event()
        self.address = ""

    async def connect(self) -> None:
        device = await BleakScanner.find_device_by_filter(
            lambda candidate, adv: (adv.local_name or candidate.name) == self.device_name,
            timeout=self.timeout,
        )
        if device is None:
            raise RuntimeError(f"BLE device {self.device_name!r} not found")
        self.address = device.address
        self.client = BleakClient(device, timeout=self.timeout, pair=False, winrt={"use_cached_services": False})
        await self.client.connect()
        if not self.client.is_connected:
            raise RuntimeError("BLE connection failed")
        resolved = resolve_characteristics(self.client, self.write_uuid, self.notify_uuid)
        self.write_char = resolved.write_char
        self.notify_char = resolved.notify_char
        await self.client.start_notify(self.notify_char, self._on_notify)

    async def close(self) -> None:
        if self.client is not None:
            if self.notify_char is not None and self.client.is_connected:
                await self.client.stop_notify(self.notify_char)
            if self.client.is_connected:
                await self.client.disconnect()

    def _on_notify(self, _: BleakGATTCharacteristic, data: bytearray) -> None:
        self.rx_buffer.extend(bytes(data))
        for frame in pop_frames(self.rx_buffer):
            self.frames.append(frame)
        self.event.set()

    async def send_frame(self, frame: bytes) -> None:
        if self.client is None or self.write_char is None:
            raise RuntimeError("BLE client is not connected")
        response = "write" in self.write_char.properties
        await self.client.write_gatt_char(self.write_char, frame, response=response)

    async def wait_for_cmd(self, cmd: int, timeout: float) -> bytes:
        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            for index, frame in enumerate(self.frames):
                if frame_cmd(frame) == cmd:
                    del self.frames[index]
                    return frame
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0:
                raise TimeoutError(f"Timed out waiting for BLE cmd=0x{cmd:02X}")
            self.event.clear()
            await asyncio.wait_for(self.event.wait(), timeout=remaining)


async def run_ble_burst(client: BleProtocolClient, mac: bytes, count: int, timeout: float) -> None:
    await client.send_frame(build_frame(CMD_HANDSHAKE, mac))
    await client.wait_for_cmd(CMD_HANDSHAKE, timeout)
    for index in range(count):
        cmd = CMD_HEARTBEAT if (index % 2) == 0 else CMD_DEV_INFO
        await client.send_frame(build_frame(cmd))
        await client.wait_for_cmd(cmd, timeout)
    print(f"[ble] protocol burst ok count={count}")


def run_mqtt_burst(service: LocalIotService, topic: str, count: int, timeout: float, publish_gap: float) -> None:
    for index in range(count):
        cmd = CMD_HEARTBEAT if (index % 2) == 0 else CMD_DEV_INFO
        sent = service.publish_to_subscribers(topic, build_frame(cmd))
        if sent == 0:
            raise RuntimeError(f"No MQTT subscriber for {topic}")
        if publish_gap > 0.0:
            time.sleep(publish_gap)
    records = service.wait_for_messages(count, timeout)
    valid = 0
    for record in records:
        frames = pop_frames(bytearray(record.payload))
        if frames:
            valid += 1
    if valid != count:
        raise RuntimeError(f"MQTT reply loss: expected={count} valid={valid}")
    print(f"[mqtt] protocol burst ok count={count}")


async def main_async(args: argparse.Namespace) -> int:
    service = LocalIotService(args.bind_host, args.http_port, args.mqtt_port, args.device_secret)
    service.start()
    rtt = RttClient(args.rtt_host, args.rtt_port, args.rtt_timeout)
    ble = BleProtocolClient(args.ble_name, args.write_uuid, args.notify_uuid, args.ble_timeout)
    try:
        rtt.send_command("iot select wifi")
        try:
            auth = service.wait_for_auth(timeout=args.auth_timeout)
            print(f"[flow] auth deviceId={auth.device_id}")
        except TimeoutError:
            print("[flow] no HTTP auth observed, continuing with cached key")
        rtt.wait_for_ready(timeout=args.ready_timeout, poll_interval=args.status_poll)
        topic = service.wait_for_subscription(timeout=args.ready_timeout, topic=args.mqtt_sub_topic)
        print(f"[flow] mqtt subscriber ready topic={topic}")

        if args.publish_prefix:
            for index in range(1, args.warmup_publish_count + 1):
                publish_with_retry(rtt, service, f"{args.publish_prefix}{index}", args.ready_timeout, args.status_poll)

        await ble.connect()
        mac = mac_text_to_bytes(args.ble_mac or ble.address)
        await asyncio.gather(
            run_ble_burst(ble, mac, args.packet_count, args.ble_timeout),
            asyncio.to_thread(run_mqtt_burst, service, topic, args.packet_count, args.ready_timeout, args.mqtt_publish_gap),
        )
        print("[result] concurrent BLE and MQTT protocol test passed")
        return 0
    finally:
        await ble.close()
        rtt.close()
        service.stop()


def main() -> int:
    parser = argparse.ArgumentParser(description="Concurrent BLE + MQTT CPR protocol reliability test")
    parser.add_argument("--bind-host", default="0.0.0.0")
    parser.add_argument("--http-port", type=int, default=8800)
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--device-secret", default="LOCAL-CPR-SECRET")
    parser.add_argument("--rtt-host", default="127.0.0.1")
    parser.add_argument("--rtt-port", type=int, default=19021)
    parser.add_argument("--rtt-timeout", type=float, default=5.0)
    parser.add_argument("--ready-timeout", type=float, default=90.0)
    parser.add_argument("--auth-timeout", type=float, default=20.0)
    parser.add_argument("--status-poll", type=float, default=2.0)
    parser.add_argument("--ble-name", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--write-uuid", default=DEFAULT_WRITE_UUID)
    parser.add_argument("--notify-uuid", default=DEFAULT_NOTIFY_UUID)
    parser.add_argument("--ble-timeout", type=float, default=12.0)
    parser.add_argument("--ble-mac", help="BLE MAC used by the CPR handshake; defaults to the BLE address")
    parser.add_argument("--mqtt-sub-topic", help="Expected firmware MQTT command subscription topic")
    parser.add_argument("--packet-count", type=int, default=20)
    parser.add_argument("--mqtt-publish-gap", type=float, default=0.02, help="gap in seconds between continuous MQTT host publishes")
    parser.add_argument("--warmup-publish-count", type=int, default=1)
    parser.add_argument("--publish-prefix", default="local-iot-warmup")
    args = parser.parse_args()
    start = time.time()
    result = asyncio.run(main_async(args))
    print(f"[result] elapsed={time.time() - start:.1f}s")
    return result


if __name__ == "__main__":
    raise SystemExit(main())