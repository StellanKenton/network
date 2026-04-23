import argparse
import asyncio
from dataclasses import dataclass
from typing import Iterable

from bleak import BleakClient
from bleak import BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic


DEFAULT_DEVICE_NAME = "Primedic-VENT-0001"
DEFAULT_SCAN_TIMEOUT = 8.0
DEFAULT_ECHO_TIMEOUT = 5.0
MAX_TEST_PAYLOAD = 256
SAFE_PAYLOAD_ALPHABET = b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.:/"
DEFAULT_WRITE_UUID = "0000c304-0000-1000-8000-00805f9b34fb"
DEFAULT_NOTIFY_UUID = "0000c305-0000-1000-8000-00805f9b34fb"


@dataclass
class ResolvedChars:
    write_char: BleakGATTCharacteristic
    notify_char: BleakGATTCharacteristic


class EchoCollector:
    def __init__(self) -> None:
        self._buffer = bytearray()
        self._event = asyncio.Event()

    def clear(self) -> None:
        self._buffer.clear()
        self._event.clear()

    def feed(self, _: BleakGATTCharacteristic, data: bytearray) -> None:
        self._buffer.extend(bytes(data))
        self._event.set()

    async def read_exact(self, expected_len: int, timeout: float) -> bytes:
        loop = asyncio.get_running_loop()
        deadline = loop.time() + timeout

        while len(self._buffer) < expected_len:
            remaining = deadline - loop.time()
            if remaining <= 0:
                raise TimeoutError(f"Timed out waiting for {expected_len} echo bytes")

            self._event.clear()
            await asyncio.wait_for(self._event.wait(), timeout=remaining)

        data = bytes(self._buffer[:expected_len])
        del self._buffer[:expected_len]
        if not self._buffer:
            self._event.clear()
        return data


def build_safe_payload(length: int) -> bytes:
    if length < 1 or length > MAX_TEST_PAYLOAD:
        raise ValueError(f"length must be in 1..{MAX_TEST_PAYLOAD}")

    data = bytearray(length)
    for index in range(length):
        data[index] = SAFE_PAYLOAD_ALPHABET[index % len(SAFE_PAYLOAD_ALPHABET)]
    return bytes(data)


def parse_hex_payload(text: str) -> bytes:
    normalized = text.replace(" ", "")
    if len(normalized) % 2 != 0:
        raise ValueError("hex payload must contain an even number of digits")
    return bytes.fromhex(normalized)


def contains_unsafe_uart_delimiters(data: bytes) -> bool:
    return any(value in data for value in (0x0A, 0x0D, 0x2C))


def score_characteristic(char: BleakGATTCharacteristic, required_props: Iterable[str]) -> tuple[int, str]:
    properties = set(char.properties)
    required = set(required_props)
    if not required.issubset(properties):
        return (-1, char.uuid)

    score = 0
    if char.service_uuid not in {"00001800-0000-1000-8000-00805f9b34fb", "00001801-0000-1000-8000-00805f9b34fb"}:
        score += 10
    if "write" in properties:
        score += 2
    if "notify" in properties:
        score += 2
    if "indicate" in properties:
        score += 1
    return (score, char.uuid)


def resolve_characteristics(
    client: BleakClient,
    write_uuid: str | None,
    notify_uuid: str | None,
) -> ResolvedChars:
    services = client.services
    write_char = None
    notify_char = None

    if write_uuid is not None:
        write_char = services.get_characteristic(write_uuid)
        if write_char is None:
            raise RuntimeError(f"Write characteristic {write_uuid} not found")
    if notify_uuid is not None:
        notify_char = services.get_characteristic(notify_uuid)
        if notify_char is None:
            raise RuntimeError(f"Notify characteristic {notify_uuid} not found")

    if write_char is None or notify_char is None:
        for service in services:
            chars = list(service.characteristics)
            if service.uuid in {"00001800-0000-1000-8000-00805f9b34fb", "00001801-0000-1000-8000-00805f9b34fb"}:
                continue

            if write_char is None:
                for char in chars:
                    if "write" in char.properties or "write-without-response" in char.properties:
                        write_char = char
                        break

            if notify_char is None and write_char is not None:
                write_seen = False
                for char in chars:
                    if char.uuid == write_char.uuid:
                        write_seen = True
                        continue
                    if write_seen and ("notify" in char.properties or "indicate" in char.properties):
                        notify_char = char
                        break
                if notify_char is None:
                    for char in chars:
                        if char.uuid != write_char.uuid and ("notify" in char.properties or "indicate" in char.properties):
                            notify_char = char
                            break

            if write_char is not None and notify_char is not None:
                break

    if write_char is None:
        for service in services:
            for char in service.characteristics:
                if "write" in char.properties or "write-without-response" in char.properties:
                    write_char = char
                    break
            if write_char is not None:
                break

    if notify_char is None:
        for service in services:
            for char in service.characteristics:
                if "notify" in char.properties or "indicate" in char.properties:
                    notify_char = char
                    break
            if notify_char is not None:
                break

    if write_char is None or notify_char is None:
        raise RuntimeError("Unable to resolve write/notify characteristics automatically")

    return ResolvedChars(write_char=write_char, notify_char=notify_char)


async def discover_device(name: str, timeout: float):
    print(f"Scanning for {name!r} for up to {timeout:.1f}s...")
    devices = await BleakScanner.discover(timeout=timeout, return_adv=True)
    for _, (device, adv) in devices.items():
        local_name = adv.local_name or device.name
        if local_name == name:
            print(f"Found {name!r} at {device.address}")
            return device
    raise RuntimeError(f"Device {name!r} not found")


async def send_and_expect_echo(
    client: BleakClient,
    resolved: ResolvedChars,
    collector: EchoCollector,
    payload: bytes,
    timeout: float,
) -> None:
    if len(payload) < 1 or len(payload) > MAX_TEST_PAYLOAD:
        raise ValueError(f"payload length must be in 1..{MAX_TEST_PAYLOAD}")

    collector.clear()
    response = "write" in resolved.write_char.properties
    await client.write_gatt_char(resolved.write_char, payload, response=response)
    echoed = await collector.read_exact(len(payload), timeout)
    if echoed != payload:
        raise RuntimeError(
            f"echo mismatch: sent={payload!r} received={echoed!r}"
        )


async def run_auto_check(
    client: BleakClient,
    resolved: ResolvedChars,
    collector: EchoCollector,
    start_size: int,
    end_size: int,
    timeout: float,
) -> None:
    for size in range(start_size, end_size + 1):
        payload = build_safe_payload(size)
        await send_and_expect_echo(client, resolved, collector, payload, timeout)
        print(f"echo ok size={size}")


async def interactive_shell(
    client: BleakClient,
    resolved: ResolvedChars,
    collector: EchoCollector,
    timeout: float,
) -> None:
    print("Interactive commands: send <text>, sendhex <hex>, size <n>, range <start> <end>, quit")
    print("Note: the current firmware loopback is text-safe only; raw bytes containing ',', CR, or LF are not reliable.")

    while True:
        try:
            line = await asyncio.to_thread(input, "ble> ")
        except EOFError:
            print()
            return

        command = line.strip()
        if not command:
            continue
        if command in {"quit", "exit"}:
            return
        if command.startswith("send "):
            payload = command[5:].encode("utf-8")
        elif command.startswith("sendhex "):
            payload = parse_hex_payload(command[8:])
        elif command.startswith("size "):
            payload = build_safe_payload(int(command[5:].strip()))
        elif command.startswith("range "):
            parts = command.split()
            if len(parts) != 3:
                print("usage: range <start> <end>")
                continue
            await run_auto_check(client, resolved, collector, int(parts[1]), int(parts[2]), timeout)
            continue
        else:
            print("unknown command")
            continue

        if contains_unsafe_uart_delimiters(payload):
            print("payload contains ',', CR, or LF; current firmware echo parser does not guarantee these bytes")
            continue

        await send_and_expect_echo(client, resolved, collector, payload, timeout)
        print(f"echo ok payload_len={len(payload)} payload={payload!r}")


async def main_async(args: argparse.Namespace) -> None:
    device = await discover_device(args.name, args.scan_timeout)
    collector = EchoCollector()

    async with BleakClient(device, timeout=args.connect_timeout) as client:
        resolved = resolve_characteristics(client, args.write_uuid, args.notify_uuid)
        print(f"Connected to {device.address}")
        print(f"Write characteristic:  {resolved.write_char.uuid} {sorted(resolved.write_char.properties)}")
        print(f"Notify characteristic: {resolved.notify_char.uuid} {sorted(resolved.notify_char.properties)}")

        await client.start_notify(resolved.notify_char, collector.feed)
        try:
            if args.auto_check:
                await run_auto_check(client, resolved, collector, 1, MAX_TEST_PAYLOAD, args.echo_timeout)
            await interactive_shell(client, resolved, collector, args.echo_timeout)
        finally:
            await client.stop_notify(resolved.notify_char)


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="ESP32-C5 BLE echo validation tool")
    parser.add_argument("--name", default=DEFAULT_DEVICE_NAME, help="advertised BLE local name")
    parser.add_argument("--scan-timeout", type=float, default=DEFAULT_SCAN_TIMEOUT, help="scan timeout in seconds")
    parser.add_argument("--connect-timeout", type=float, default=10.0, help="connect timeout in seconds")
    parser.add_argument("--echo-timeout", type=float, default=DEFAULT_ECHO_TIMEOUT, help="echo wait timeout in seconds")
    parser.add_argument("--write-uuid", default=DEFAULT_WRITE_UUID, help="write characteristic UUID")
    parser.add_argument("--notify-uuid", default=DEFAULT_NOTIFY_UUID, help="notify characteristic UUID")
    parser.add_argument("--no-auto-check", action="store_true", help="skip the 1..256 byte automatic echo sweep")
    return parser


def main() -> None:
    parser = build_arg_parser()
    args = parser.parse_args()
    args.auto_check = not args.no_auto_check
    asyncio.run(main_async(args))


if __name__ == "__main__":
    main()