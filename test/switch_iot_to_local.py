from __future__ import annotations

import argparse
import json
import re
import socket
from pathlib import Path


ROOT_DIR = Path(__file__).resolve().parents[1]
WIRELESS_HEADER = ROOT_DIR / "net" / "User" / "manager" / "wireless" / "wireless.h"
BACKUP_FILE = ROOT_DIR / "test" / "local_iot_header_backup.json"

MACRO_PATTERNS = {
    "WIRELESS_IOT_LOCAL_TEST_MODE": re.compile(r'^(#define\s+WIRELESS_IOT_LOCAL_TEST_MODE\s+).+$', re.MULTILINE),
    "WIRELESS_IOT_LOCAL_HTTP_URL": re.compile(r'^(#define\s+WIRELESS_IOT_LOCAL_HTTP_URL\s+).+$', re.MULTILINE),
    "WIRELESS_IOT_LOCAL_MQTT_HOST": re.compile(r'^(#define\s+WIRELESS_IOT_LOCAL_MQTT_HOST\s+).+$', re.MULTILINE),
    "WIRELESS_IOT_LOCAL_MQTT_PORT": re.compile(r'^(#define\s+WIRELESS_IOT_LOCAL_MQTT_PORT\s+).+$', re.MULTILINE),
}


def detect_local_ip() -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("8.8.8.8", 80))
        return probe.getsockname()[0]
    except OSError:
        return socket.gethostbyname(socket.gethostname())
    finally:
        probe.close()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.write_text(text, encoding="utf-8", newline="\n")


def snapshot_macros(text: str) -> dict[str, str]:
    snapshot: dict[str, str] = {}
    for name, pattern in MACRO_PATTERNS.items():
        match = pattern.search(text)
        if match is None:
            raise RuntimeError(f"macro {name} not found in {WIRELESS_HEADER}")
        snapshot[name] = match.group(0)
    return snapshot


def replace_macro(text: str, name: str, value: str) -> str:
    pattern = MACRO_PATTERNS[name]
    updated, count = pattern.subn(lambda match: f"{match.group(1)}{value}", text, count=1)
    if count != 1:
        raise RuntimeError(f"failed replacing macro {name}")
    return updated


def apply_local_mode(ip_address: str, http_port: int, mqtt_port: int) -> None:
    original = read_text(WIRELESS_HEADER)
    if not BACKUP_FILE.exists():
        BACKUP_FILE.write_text(json.dumps(snapshot_macros(original), indent=2, ensure_ascii=True), encoding="utf-8", newline="\n")

    updated = original
    updated = replace_macro(updated, "WIRELESS_IOT_LOCAL_TEST_MODE", "1U")
    updated = replace_macro(updated, "WIRELESS_IOT_LOCAL_HTTP_URL", f'"http://{ip_address}:{http_port}/device/secret-key"')
    updated = replace_macro(updated, "WIRELESS_IOT_LOCAL_MQTT_HOST", f'"{ip_address}"')
    updated = replace_macro(updated, "WIRELESS_IOT_LOCAL_MQTT_PORT", f'"{mqtt_port}"')
    write_text(WIRELESS_HEADER, updated)
    print(f"Applied local IoT override with host {ip_address}, http={http_port}, mqtt={mqtt_port}")


def restore_original() -> None:
    if not BACKUP_FILE.exists():
        raise FileNotFoundError(f"backup file not found: {BACKUP_FILE}")

    current = read_text(WIRELESS_HEADER)
    backup = json.loads(BACKUP_FILE.read_text(encoding="utf-8"))
    updated = current
    for name, original_line in backup.items():
        updated = replace_macro(updated, name, original_line.split(None, 2)[2])
    write_text(WIRELESS_HEADER, updated)
    print("Restored original local IoT macros")


def main() -> int:
    parser = argparse.ArgumentParser(description="Switch wireless local IoT macros between real and local mock endpoints")
    parser.add_argument("action", choices=["apply", "restore", "show-ip"])
    parser.add_argument("--ip", help="local IPv4 address visible to the device")
    parser.add_argument("--http-port", type=int, default=8800)
    parser.add_argument("--mqtt-port", type=int, default=1883)
    args = parser.parse_args()

    if args.action == "show-ip":
        print(args.ip or detect_local_ip())
        return 0

    if args.action == "restore":
        restore_original()
        return 0

    apply_local_mode(args.ip or detect_local_ip(), args.http_port, args.mqtt_port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())