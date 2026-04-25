from __future__ import annotations

import argparse
import asyncio
import re
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
if str(TEST_DIR) not in sys.path:
    sys.path.insert(0, str(TEST_DIR))

from concurrent_ble_mqtt_protocol_test import (
    CMD_HANDSHAKE,
    CMD_HEARTBEAT,
    BleProtocolClient,
    build_frame,
    mac_text_to_bytes,
    pop_frames,
)
from esp32c5_ble_tester import DEFAULT_DEVICE_NAME, DEFAULT_NOTIFY_UUID, DEFAULT_WRITE_UUID
from local_iot_service import LocalIotService
from local_iot_workflow import RttClient


STATUS_PATTERN = re.compile(
    r"bleEnabled=(?P<ble_enabled>[01])\s+ble=(?P<ble>\S+)\s+"
    r"wifiEnabled=(?P<wifi_enabled>[01])\s+wifi=(?P<wifi>\S+)\s+"
    r"mqttEnabled=(?P<mqtt_enabled>[01])\s+iot=(?P<iot>\S+)\s+ready=(?P<ready>[01])"
)


@dataclass
class HeartbeatSnapshot:
    seq: int
    timestamp: float
    last_error: str


class HeartbeatStats:
    def __init__(self, name: str) -> None:
        self.name = name
        self._lock = threading.Lock()
        self._seq = 0
        self._last_ok = 0.0
        self._last_error = ""

    def mark_ok(self) -> int:
        with self._lock:
            self._seq += 1
            self._last_ok = time.monotonic()
            self._last_error = ""
            return self._seq

    def mark_error(self, error: BaseException) -> None:
        with self._lock:
            self._last_error = repr(error)

    def snapshot(self) -> HeartbeatSnapshot:
        with self._lock:
            return HeartbeatSnapshot(self._seq, self._last_ok, self._last_error)


class MqttHeartbeatWorker:
    def __init__(
        self,
        service: LocalIotService,
        expected_topic: str | None,
        enabled: threading.Event,
        stop: threading.Event,
        stats: HeartbeatStats,
        interval: float,
        reply_timeout: float,
    ) -> None:
        self.service = service
        self.expected_topic = expected_topic
        self.enabled = enabled
        self.stop = stop
        self.stats = stats
        self.interval = interval
        self.reply_timeout = reply_timeout
        self.topic: str | None = None
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self) -> None:
        self.thread.start()

    def join(self) -> None:
        self.thread.join(timeout=5.0)

    def run(self) -> None:
        heartbeat_frame = build_frame(CMD_HEARTBEAT)
        while not self.stop.is_set():
            if not self.enabled.is_set():
                self.stop.wait(0.1)
                continue

            try:
                if self.topic is None:
                    self.topic = self.service.wait_for_subscription(timeout=2.0, topic=self.expected_topic)

                sent_count = self.service.publish_to_subscribers(self.topic, heartbeat_frame)
                if sent_count == 0:
                    self.topic = None
                    self.stop.wait(0.2)
                    continue

                record = self.service.wait_for_messages(expected_count=1, timeout=self.reply_timeout)[0]
                frames = pop_frames(bytearray(record.payload))
                if len(frames) != 1 or frames[0][3] != CMD_HEARTBEAT:
                    raise RuntimeError(f"invalid MQTT heartbeat payload={record.payload.hex()}")

                seq = self.stats.mark_ok()
                print(f"[mqtt-heartbeat] seq={seq} ok topic={record.topic}")
                self.stop.wait(self.interval)
            except Exception as error:
                self.stats.mark_error(error)
                if self.enabled.is_set():
                    print(f"[mqtt-heartbeat] transient error: {error!r}")
                self.topic = None
                self.stop.wait(0.3)


class BleHeartbeatWorker:
    def __init__(
        self,
        client: BleProtocolClient,
        active: asyncio.Event,
        stop: asyncio.Event,
        stats: HeartbeatStats,
        interval: float,
        timeout: float,
        mac_text: str | None,
    ) -> None:
        self.client = client
        self.active = active
        self.stop = stop
        self.stats = stats
        self.interval = interval
        self.timeout = timeout
        self.mac_text = mac_text

    async def run(self) -> None:
        while not self.stop.is_set():
            if not self.active.is_set():
                await self._close_client()
                await asyncio.sleep(0.1)
                continue

            try:
                await self._ensure_connected_and_handshaken()
                await self.client.send_frame(build_frame(CMD_HEARTBEAT))
                await self.client.wait_for_cmd(CMD_HEARTBEAT, self.timeout)
                seq = self.stats.mark_ok()
                print(f"[ble-heartbeat] seq={seq} ok")
                await asyncio.sleep(self.interval)
            except Exception as error:
                self.stats.mark_error(error)
                if self.active.is_set():
                    print(f"[ble-heartbeat] transient error: {error!r}")
                await self._close_client()
                await asyncio.sleep(0.5)

    async def _ensure_connected_and_handshaken(self) -> None:
        if self.client.client is None or not self.client.client.is_connected:
            await self.client.connect()
            mac = mac_text_to_bytes(self.mac_text or self.client.address)
            await self.client.send_frame(build_frame(CMD_HANDSHAKE, mac))
            await self.client.wait_for_cmd(CMD_HANDSHAKE, self.timeout)

    async def _close_client(self) -> None:
        await self.client.close()
        self.client.client = None
        self.client.write_char = None
        self.client.notify_char = None
        self.client.rx_buffer.clear()
        self.client.frames.clear()


def command_status(rtt: RttClient, command: str, settle: float = 1.0) -> tuple[dict[str, str], str]:
    start_len = len(rtt._buffer)
    rtt.send_command(command, settle=settle)
    output = rtt._buffer[start_len:]
    for _ in range(8):
        if STATUS_PATTERN.search(output) and ("OK" in output or "ERROR" in output):
            break
        time.sleep(0.5)
        rtt.read_available()
        output = rtt._buffer[start_len:]

    matches = list(STATUS_PATTERN.finditer(output))
    if not matches:
        raise AssertionError(f"No wireless status in response to {command!r}: {output!r}")
    status = matches[-1].groupdict()
    print(f"[status] {command}: {status}")
    return status, output


def command_status_ok(rtt: RttClient, command: str, attempts: int = 6, settle: float = 1.0) -> dict[str, str]:
    last_output = ""
    for attempt in range(1, attempts + 1):
        status, output = command_status(rtt, command, settle=settle)
        last_output = output
        if "OK" in output:
            return status
        print(f"[flow] command retry attempt={attempt} command={command!r}")
        time.sleep(1.0)
    raise AssertionError(f"Command {command!r} did not return OK after {attempts} attempts: {last_output!r}")


def wait_for_status(rtt: RttClient, predicate, timeout: float, poll_interval: float, label: str) -> dict[str, str]:
    deadline = time.monotonic() + timeout
    last_status: dict[str, str] | None = None
    while time.monotonic() < deadline:
        last_status = command_status_ok(rtt, "wireless status", attempts=1, settle=0.5)
        if predicate(last_status):
            return last_status
        time.sleep(poll_interval)
    raise TimeoutError(f"Timed out waiting for {label}; last={last_status}")


def wait_for_mqtt_ready(rtt: RttClient, timeout: float, poll_interval: float) -> dict[str, str]:
    deadline = time.monotonic() + timeout
    last_status: dict[str, str] | None = None
    while time.monotonic() < deadline:
        last_status = command_status_ok(rtt, "wireless status", attempts=1, settle=0.5)
        if last_status["iot"] == "mqtt-ready":
            return last_status
        if last_status["iot"] == "error":
            print("[flow] iot state is error, arming retry")
            rtt.send_command("iot retry", settle=0.8)
        time.sleep(poll_interval)
    raise TimeoutError(f"Timed out waiting for MQTT ready; last={last_status}")


async def wait_for_progress(stats: HeartbeatStats, after_seq: int, timeout: float, label: str) -> HeartbeatSnapshot:
    deadline = time.monotonic() + timeout
    snapshot = stats.snapshot()
    while time.monotonic() < deadline:
        snapshot = stats.snapshot()
        if snapshot.seq > after_seq:
            return snapshot
        await asyncio.sleep(0.1)
    raise TimeoutError(f"Timed out waiting for {label} heartbeat; last={snapshot}")


async def assert_both_heartbeat(
    ble_stats: HeartbeatStats,
    mqtt_stats: HeartbeatStats,
    timeout: float,
    label: str,
) -> tuple[HeartbeatSnapshot, HeartbeatSnapshot]:
    ble_before = ble_stats.snapshot()
    mqtt_before = mqtt_stats.snapshot()
    ble_after, mqtt_after = await asyncio.gather(
        wait_for_progress(ble_stats, ble_before.seq, timeout, f"BLE {label}"),
        wait_for_progress(mqtt_stats, mqtt_before.seq, timeout, f"MQTT {label}"),
    )
    print(f"[verify] {label}: ble_seq={ble_after.seq} mqtt_seq={mqtt_after.seq}")
    return ble_after, mqtt_after


async def run_switch_cycle(
    args: argparse.Namespace,
    rtt: RttClient,
    ble_active: asyncio.Event,
    mqtt_active: threading.Event,
    ble_stats: HeartbeatStats,
    mqtt_stats: HeartbeatStats,
    cycle_index: int,
) -> None:
    print(f"[cycle] start index={cycle_index}")
    await assert_both_heartbeat(ble_stats, mqtt_stats, args.progress_timeout, f"cycle {cycle_index} baseline")

    mqtt_before = mqtt_stats.snapshot()
    await asyncio.to_thread(command_status_ok, rtt, "wireless ble_off")
    ble_active.clear()
    await wait_for_progress(mqtt_stats, mqtt_before.seq, args.progress_timeout, "MQTT while BLE off")

    await asyncio.to_thread(command_status_ok, rtt, "wireless ble_on")
    ble_active.set()
    await assert_both_heartbeat(ble_stats, mqtt_stats, args.progress_timeout, f"cycle {cycle_index} after BLE reopen")

    ble_before = ble_stats.snapshot()
    await asyncio.to_thread(command_status_ok, rtt, "wireless mqtt_off")
    mqtt_active.clear()
    await wait_for_progress(ble_stats, ble_before.seq, args.progress_timeout, "BLE while MQTT off")

    await asyncio.to_thread(command_status_ok, rtt, "wireless mqtt_on")
    await asyncio.to_thread(wait_for_mqtt_ready, rtt, args.ready_timeout, args.status_poll)
    mqtt_active.set()
    await assert_both_heartbeat(ble_stats, mqtt_stats, args.progress_timeout, f"cycle {cycle_index} after MQTT reopen")

    ble_before = ble_stats.snapshot()
    await asyncio.to_thread(command_status_ok, rtt, "wireless wifi_off", 10, 1.2)
    mqtt_active.clear()
    await wait_for_progress(ble_stats, ble_before.seq, args.progress_timeout, "BLE while WiFi off")

    await asyncio.to_thread(command_status_ok, rtt, "wireless wifi_on")
    await asyncio.to_thread(command_status_ok, rtt, f"wireless wifi_connect {args.ssid} {args.password}")
    await asyncio.to_thread(
        wait_for_status,
        rtt,
        lambda item: item["wifi"] == "connected",
        args.ready_timeout,
        args.status_poll,
        "WiFi connected after reopen",
    )
    await asyncio.to_thread(command_status_ok, rtt, "wireless mqtt_on")
    await asyncio.to_thread(wait_for_mqtt_ready, rtt, args.ready_timeout, args.status_poll)
    mqtt_active.set()
    await assert_both_heartbeat(ble_stats, mqtt_stats, args.progress_timeout, f"cycle {cycle_index} after WiFi reopen")


async def main_async(args: argparse.Namespace) -> int:
    if not args.ssid:
        raise ValueError("--ssid is required because the WiFi off/on cycle must reconnect explicitly")

    service = LocalIotService(args.bind_host, args.http_port, args.mqtt_port, args.device_secret)
    service.start()
    rtt = RttClient(args.rtt_host, args.rtt_port, args.rtt_timeout)
    ble_client = BleProtocolClient(args.ble_name, args.write_uuid, args.notify_uuid, args.ble_timeout)
    ble_active = asyncio.Event()
    ble_stop = asyncio.Event()
    mqtt_active = threading.Event()
    mqtt_stop = threading.Event()
    ble_stats = HeartbeatStats("ble")
    mqtt_stats = HeartbeatStats("mqtt")
    ble_worker = BleHeartbeatWorker(
        ble_client,
        ble_active,
        ble_stop,
        ble_stats,
        args.heartbeat_interval,
        args.ble_timeout,
        args.ble_mac,
    )
    mqtt_worker = MqttHeartbeatWorker(
        service,
        args.mqtt_sub_topic,
        mqtt_active,
        mqtt_stop,
        mqtt_stats,
        args.heartbeat_interval,
        args.reply_timeout,
    )
    ble_task = asyncio.create_task(ble_worker.run())
    mqtt_worker.start()

    try:
        await asyncio.to_thread(command_status_ok, rtt, "wireless ble_on")
        await asyncio.to_thread(command_status_ok, rtt, "wireless wifi_on")
        await asyncio.to_thread(command_status_ok, rtt, f"wireless wifi_connect {args.ssid} {args.password}")
        await asyncio.to_thread(
            wait_for_status,
            rtt,
            lambda item: item["wifi"] == "connected",
            args.ready_timeout,
            args.status_poll,
            "initial WiFi connected",
        )
        await asyncio.to_thread(command_status_ok, rtt, "wireless mqtt_on")
        await asyncio.to_thread(wait_for_mqtt_ready, rtt, args.ready_timeout, args.status_poll)

        ble_active.set()
        mqtt_active.set()
        await assert_both_heartbeat(ble_stats, mqtt_stats, args.progress_timeout, "initial")

        for cycle_index in range(1, args.cycles + 1):
            await run_switch_cycle(args, rtt, ble_active, mqtt_active, ble_stats, mqtt_stats, cycle_index)

        print(
            f"[result] wireless switch heartbeat stress passed "
            f"ble_seq={ble_stats.snapshot().seq} mqtt_seq={mqtt_stats.snapshot().seq} cycles={args.cycles}"
        )
        return 0
    finally:
        ble_active.clear()
        mqtt_active.clear()
        ble_stop.set()
        mqtt_stop.set()
        await ble_client.close()
        try:
            await asyncio.wait_for(ble_task, timeout=5.0)
        except TimeoutError:
            ble_task.cancel()
        mqtt_worker.join()
        rtt.close()
        service.stop()


def main() -> int:
    parser = argparse.ArgumentParser(description="Stress BLE/WiFi/MQTT switches while BLE and MQTT heartbeats are active")
    parser.add_argument("--bind-host", default="0.0.0.0")
    parser.add_argument("--http-port", type=int, default=8800)
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--device-secret", default="LOCAL-CPR-SECRET")
    parser.add_argument("--rtt-host", default="127.0.0.1")
    parser.add_argument("--rtt-port", type=int, default=19021)
    parser.add_argument("--rtt-timeout", type=float, default=5.0)
    parser.add_argument("--ready-timeout", type=float, default=90.0)
    parser.add_argument("--status-poll", type=float, default=2.0)
    parser.add_argument("--ssid", required=True)
    parser.add_argument("--password", default="")
    parser.add_argument("--ble-name", default=DEFAULT_DEVICE_NAME)
    parser.add_argument("--write-uuid", default=DEFAULT_WRITE_UUID)
    parser.add_argument("--notify-uuid", default=DEFAULT_NOTIFY_UUID)
    parser.add_argument("--ble-timeout", type=float, default=12.0)
    parser.add_argument("--ble-mac", help="BLE MAC used by the CPR handshake; defaults to the BLE address")
    parser.add_argument("--mqtt-sub-topic", help="Expected firmware MQTT command subscription topic")
    parser.add_argument("--heartbeat-interval", type=float, default=0.25)
    parser.add_argument("--reply-timeout", type=float, default=3.0)
    parser.add_argument("--progress-timeout", type=float, default=20.0)
    parser.add_argument("--cycles", type=int, default=2)
    args = parser.parse_args()
    start = time.time()
    result = asyncio.run(main_async(args))
    print(f"[result] elapsed={time.time() - start:.1f}s")
    return result


if __name__ == "__main__":
    raise SystemExit(main())