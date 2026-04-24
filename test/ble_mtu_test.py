import asyncio
import sys
from typing import Optional

from bleak import BleakClient, BleakScanner


DEVICE_NAME = "Primedic-VENT-0001"
SCAN_TIMEOUT_SECONDS = 12.0
CONNECT_TIMEOUT_SECONDS = 30.0
MTU_POLL_COUNT = 15
MTU_POLL_INTERVAL_SECONDS = 0.5


async def find_device_by_name(name: str):
    print(f"Scanning for BLE device named {name!r}...")
    device = await BleakScanner.find_device_by_filter(
        lambda detected_device, adv: detected_device.name == name
        or adv.local_name == name,
        timeout=SCAN_TIMEOUT_SECONDS,
    )
    return device


def get_max_wwr_sizes(client: BleakClient) -> list[tuple[str, str, int]]:
    sizes = []
    for service in client.services:
        for char in service.characteristics:
            if "write-without-response" in char.properties:
                sizes.append((service.uuid, char.uuid, char.max_write_without_response_size))
    return sizes


async def main() -> int:
    device = await find_device_by_name(DEVICE_NAME)
    if device is None:
        print(f"RESULT: device {DEVICE_NAME!r} not found")
        return 2

    print(f"Found: address={device.address} name={device.name!r}")

    try:
        async with BleakClient(
            device,
            timeout=CONNECT_TIMEOUT_SECONDS,
            pair=False,
            winrt={"use_cached_services": False},
        ) as client:
            print(f"Connected: {client.is_connected}")

            mtu_samples: list[int] = []
            for index in range(MTU_POLL_COUNT):
                mtu = client.mtu_size
                mtu_samples.append(mtu)
                print(f"MTU[{index}]={mtu}")
                await asyncio.sleep(MTU_POLL_INTERVAL_SECONDS)

            sizes = get_max_wwr_sizes(client)
            if sizes:
                print("write-without-response sizes:")
                for service_uuid, char_uuid, size in sizes:
                    print(f"  service={service_uuid} char={char_uuid} max_write_without_response_size={size}")
            else:
                print("No write-without-response characteristics found")

            final_mtu = mtu_samples[-1]
            if final_mtu > 23:
                print(f"RESULT: negotiated MTU is {final_mtu}, larger than default 23")
            else:
                print("RESULT: MTU stayed at 23, no larger MTU negotiation observed")

            return 0
    except Exception as exc:
        print(f"RESULT: connection or MTU check failed: {type(exc).__name__}: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))