from __future__ import annotations

import argparse
import re
import socket
import time

from local_iot_service import LocalIotService


READY_PATTERN = re.compile(r"link\[wifi\]:.*auth=1 mqtt=1")


class RttClient:
    def __init__(self, host: str, port: int, timeout: float) -> None:
        self._socket = socket.create_connection((host, port), timeout=timeout)
        self._socket.settimeout(0.5)
        self._buffer = ""

    def close(self) -> None:
        self._socket.close()

    def read_available(self) -> str:
        chunks: list[str] = []
        while True:
            try:
                data = self._socket.recv(4096)
            except TimeoutError:
                break
            if not data:
                break
            chunks.append(data.decode("utf-8", errors="replace"))
        if chunks:
            joined = "".join(chunks)
            self._buffer += joined
            print(joined, end="")
            return joined
        return ""

    def send_command(self, command: str, settle: float = 0.8) -> str:
        self._socket.sendall(command.encode("utf-8") + b"\r\n")
        time.sleep(settle)
        return self.read_available()

    def wait_for_ready(self, timeout: float, poll_interval: float) -> str:
        deadline = time.time() + timeout
        snapshot = self.read_available()
        while time.time() < deadline:
            snapshot += self.send_command("iot status", settle=0.6)
            if READY_PATTERN.search(self._buffer):
                return self._buffer
            time.sleep(poll_interval)
        raise TimeoutError("Timed out waiting for wifi auth=1 mqtt=1 in RTT output")


def publish_with_retry(rtt: RttClient, service: LocalIotService, payload: str, ready_timeout: float, poll_interval: float) -> None:
    for attempt in range(1, 6):
        print(f"[flow] waiting for mqtt ready before publish attempt={attempt} payload={payload}")
        rtt.wait_for_ready(timeout=min(ready_timeout, 30.0), poll_interval=poll_interval)
        response = rtt.send_command(f"iot pub {payload}", settle=1.0)
        if response:
            print(response, end="")
        try:
            record = service.wait_for_messages(expected_count=1, timeout=8.0)[0]
        except TimeoutError:
            print(f"[flow] publish not observed on attempt={attempt} payload={payload}")
            continue

        received_text = record.payload.decode("utf-8", errors="replace")
        print(f"[flow] observed mqtt payload topic={record.topic} payload={received_text}")
        if received_text == payload:
            return

        if received_text.startswith(payload):
            print(f"[flow] accepted payload prefix match extra={received_text[len(payload):]}")
            return

    raise TimeoutError(f"Timed out publishing payload {payload}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Run local IoT mock workflow through RTT and MQTT")
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
    parser.add_argument("--message-count", type=int, default=3)
    parser.add_argument("--publish-prefix", default="local-iot-test")
    args = parser.parse_args()

    service = LocalIotService(args.bind_host, args.http_port, args.mqtt_port, args.device_secret)
    service.start()
    rtt = RttClient(args.rtt_host, args.rtt_port, args.rtt_timeout)

    try:
        print("[flow] selecting wifi route")
        rtt.send_command("iot select wifi")
        print("[flow] waiting for HTTP auth request or cached-key reconnect")
        try:
            auth = service.wait_for_auth(timeout=min(args.auth_timeout, args.ready_timeout))
        except TimeoutError:
            print("[flow] no new HTTP auth request observed, continuing with cached-key path")
        else:
            print(f"[flow] auth received for deviceId={auth.device_id} moduleId={auth.module_id}")

        print("[flow] waiting for mqtt ready")
        rtt.wait_for_ready(timeout=args.ready_timeout, poll_interval=args.status_poll)

        for index in range(1, args.message_count + 1):
            payload = f"{args.publish_prefix}{index}"
            publish_with_retry(rtt, service, payload, args.ready_timeout, args.status_poll)
    finally:
        rtt.close()
        service.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())