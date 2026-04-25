from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
if str(TEST_DIR) not in sys.path:
    sys.path.insert(0, str(TEST_DIR))

from local_iot_service import LocalIotService
from local_iot_workflow import RttClient


FRAME_HEAD = b"\xFA\xFC\x01"
CMD_HEARTBEAT = 0x03
DEFAULT_HEARTBEAT_INTERVAL_S = 0.2


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


def wait_for_auth_or_cached_key(service: LocalIotService, timeout: float) -> None:
    try:
        auth = service.wait_for_auth(timeout=timeout)
    except TimeoutError:
        print("[flow] no new HTTP auth observed, continuing with cached-key path")
        return

    print(f"[flow] auth received for deviceId={auth.device_id} moduleId={auth.module_id}")


def run_heartbeat_loop(args: argparse.Namespace) -> int:
    service = LocalIotService(args.bind_host, args.http_port, args.mqtt_port, args.device_secret)
    service.start()
    rtt = RttClient(args.rtt_host, args.rtt_port, args.rtt_timeout)

    try:
        print("[flow] selecting wifi route")
        rtt.send_command("iot select wifi")
        print("[flow] waiting for HTTP auth request or cached-key reconnect")
        wait_for_auth_or_cached_key(service, timeout=min(args.auth_timeout, args.ready_timeout))

        print("[flow] waiting for mqtt ready")
        rtt.wait_for_ready(timeout=args.ready_timeout, poll_interval=args.status_poll)
        topic = service.wait_for_subscription(timeout=args.ready_timeout, topic=args.mqtt_sub_topic)
        print(f"[flow] mqtt subscriber ready topic={topic}")

        heartbeat_frame = build_frame(CMD_HEARTBEAT)
        sequence = 0
        next_send_time = time.monotonic()
        while True:
            if args.count > 0 and sequence >= args.count:
                break

            now = time.monotonic()
            if now < next_send_time:
                time.sleep(next_send_time - now)

            sent_count = service.publish_to_subscribers(topic, heartbeat_frame)
            if sent_count == 0:
                raise RuntimeError(f"No MQTT subscriber for {topic}")

            record = service.wait_for_messages(expected_count=1, timeout=args.reply_timeout)[0]
            frames = pop_frames(bytearray(record.payload))
            if len(frames) != 1 or frames[0][3] != CMD_HEARTBEAT:
                raise RuntimeError(
                    f"Invalid MQTT heartbeat reply topic={record.topic} payload={record.payload.hex()}"
                )

            sequence += 1
            print(f"[mqtt-heartbeat] seq={sequence} ok interval={args.heartbeat_interval:.3f}s")
            next_send_time = max(next_send_time + args.heartbeat_interval, time.monotonic())
    finally:
        rtt.close()
        service.stop()

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Run local MQTT CPR heartbeat host and validate firmware heartbeat replies")
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
    parser.add_argument("--mqtt-sub-topic", help="Expected firmware MQTT command subscription topic")
    parser.add_argument("--heartbeat-interval", type=float, default=DEFAULT_HEARTBEAT_INTERVAL_S)
    parser.add_argument("--reply-timeout", type=float, default=3.0)
    parser.add_argument("--count", type=int, default=0, help="Heartbeat count, 0 means run until interrupted")
    args = parser.parse_args()
    return run_heartbeat_loop(args)


if __name__ == "__main__":
    raise SystemExit(main())