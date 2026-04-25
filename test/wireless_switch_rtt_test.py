from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
if str(TEST_DIR) not in sys.path:
    sys.path.insert(0, str(TEST_DIR))

from local_iot_service import LocalIotService
from local_iot_workflow import RttClient


STATUS_PATTERN = re.compile(
    r"bleEnabled=(?P<ble_enabled>[01])\s+ble=(?P<ble>\S+)\s+"
    r"wifiEnabled=(?P<wifi_enabled>[01])\s+wifi=(?P<wifi>\S+)\s+"
    r"mqttEnabled=(?P<mqtt_enabled>[01])\s+iot=(?P<iot>\S+)\s+ready=(?P<ready>[01])"
)


def command_status(rtt: RttClient, command: str, settle: float = 1.0) -> dict[str, str]:
    start_len = len(rtt._buffer)
    output = rtt.send_command(command, settle=settle)
    output = rtt._buffer[start_len:]
    for _ in range(6):
        if STATUS_PATTERN.search(output) and ("OK" in output):
            break
        time.sleep(0.5)
        rtt.read_available()
        output = rtt._buffer[start_len:]
    matches = list(STATUS_PATTERN.finditer(output))
    match = matches[-1] if matches else None
    if not match:
        raise AssertionError(f"No wireless status in response to {command!r}: {output!r}")
    if "OK" not in output:
        raise AssertionError(f"Command {command!r} did not return OK: {output!r}")
    status = match.groupdict()
    print(f"[status] {command}: {status}")
    return status


def wait_for_status(rtt: RttClient, predicate, timeout: float, poll_interval: float, label: str) -> dict[str, str]:
    deadline = time.time() + timeout
    last_status: dict[str, str] | None = None
    last_error: Exception | None = None
    while time.time() < deadline:
        try:
            last_status = command_status(rtt, "wireless status", settle=0.5)
            last_error = None
            if predicate(last_status):
                return last_status
        except AssertionError as error:
            last_error = error
        time.sleep(poll_interval)
    raise TimeoutError(f"Timed out waiting for {label}; last={last_status}; error={last_error}")


def assert_field(status: dict[str, str], key: str, expected: str) -> None:
    actual = status[key]
    if actual != expected:
        raise AssertionError(f"Expected {key}={expected}, got {actual}; status={status}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate independent BLE/WiFi/MQTT RTT switch control")
    parser.add_argument("--rtt-host", default="127.0.0.1")
    parser.add_argument("--rtt-port", type=int, default=19021)
    parser.add_argument("--rtt-timeout", type=float, default=5.0)
    parser.add_argument("--ready-timeout", type=float, default=90.0)
    parser.add_argument("--status-poll", type=float, default=2.0)
    parser.add_argument("--ssid", default="")
    parser.add_argument("--password", default="")
    parser.add_argument("--bind-host", default="0.0.0.0")
    parser.add_argument("--http-port", type=int, default=8800)
    parser.add_argument("--mqtt-port", type=int, default=1883)
    parser.add_argument("--device-secret", default="LOCAL-CPR-SECRET")
    parser.add_argument("--skip-mqtt-service", action="store_true")
    args = parser.parse_args()

    service = None if args.skip_mqtt_service else LocalIotService(args.bind_host, args.http_port, args.mqtt_port, args.device_secret)
    if service is not None:
        service.start()

    rtt = RttClient(args.rtt_host, args.rtt_port, args.rtt_timeout)
    try:
        wait_for_status(rtt, lambda item: item["ready"] == "1", args.ready_timeout, args.status_poll, "wireless ready")
        command_status(rtt, "wireless mqtt_off")
        boot = command_status(rtt, "wireless ble_off")
        assert_field(boot, "ble_enabled", "0")
        assert_field(boot, "mqtt_enabled", "0")

        ble_on = command_status(rtt, "wireless ble_on")
        assert_field(ble_on, "ble_enabled", "1")
        wifi_off = command_status(rtt, "wireless wifi_off")
        assert_field(wifi_off, "wifi_enabled", "0")
        assert_field(wifi_off, "mqtt_enabled", "0")
        ble_still_on = command_status(rtt, "wireless status")
        assert_field(ble_still_on, "ble_enabled", "1")

        mqtt_fail_output = rtt.send_command("wireless mqtt_on", settle=0.8)
        if "ERR" not in mqtt_fail_output and "error" not in mqtt_fail_output.lower():
            raise AssertionError(f"mqtt_on should fail while WiFi is off: {mqtt_fail_output!r}")

        wifi_on = command_status(rtt, "wireless wifi_on")
        assert_field(wifi_on, "wifi_enabled", "1")
        assert_field(wifi_on, "mqtt_enabled", "0")
        ble_after_wifi = command_status(rtt, "wireless status")
        assert_field(ble_after_wifi, "ble_enabled", "1")

        ble_off = command_status(rtt, "wireless ble_off")
        assert_field(ble_off, "ble_enabled", "0")
        wifi_after_ble = command_status(rtt, "wireless status")
        assert_field(wifi_after_ble, "wifi_enabled", "1")

        if args.ssid:
            command_status(rtt, f"wireless wifi_connect {args.ssid} {args.password}")
            wait_for_status(rtt, lambda item: item["wifi"] == "connected", args.ready_timeout, args.status_poll, "WiFi connected")
            mqtt_on = command_status(rtt, "wireless mqtt_on")
            assert_field(mqtt_on, "mqtt_enabled", "1")
            wait_for_status(rtt, lambda item: item["iot"] == "mqtt-ready", args.ready_timeout, args.status_poll, "MQTT ready")
            mqtt_off = command_status(rtt, "wireless mqtt_off")
            assert_field(mqtt_off, "mqtt_enabled", "0")
            wifi_after_mqtt = command_status(rtt, "wireless status")
            assert_field(wifi_after_mqtt, "wifi", "connected")
            command_status(rtt, "wireless mqtt_on")
            wait_for_status(rtt, lambda item: item["iot"] == "mqtt-ready", args.ready_timeout, args.status_poll, "MQTT ready before WiFi off")
            wifi_off_final = command_status(rtt, "wireless wifi_off")
            assert_field(wifi_off_final, "wifi_enabled", "0")
            assert_field(wifi_off_final, "mqtt_enabled", "0")

        print("[result] wireless switch RTT test passed")
        return 0
    finally:
        rtt.close()
        if service is not None:
            service.stop()


if __name__ == "__main__":
    raise SystemExit(main())
