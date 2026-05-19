import asyncio
from typing import List,Callable, Optional, Sequence
from modules.ble_tcp_dataclasses import ChannelRuntime
import logging
from bleak import BleakClient, BleakScanner
from modules.pcap_logger.pcap_logger_dataclasses import LoggingParameters
from modules.tcp import forward_to_tcp
from bleak.backends.characteristic import (
    BleakGATTCharacteristic,
)

logger = logging.getLogger(__name__)


def make_notify_handler(
    channel: ChannelRuntime, logging_parameters: Optional[LoggingParameters]
) -> Callable[[BleakGATTCharacteristic, bytearray], None]:
    def handler(_: BleakGATTCharacteristic, data: bytearray):
        logger.info(f"{channel.config.name} RX {bytes(data)!r}")
        asyncio.create_task(forward_to_tcp(channel, bytes(data), logging_parameters))

    return handler


async def ble_write_worker(client: BleakClient, channel: ChannelRuntime):
    while client.is_connected:
        data = await channel.tx_queue.get()

        try:
            logger.info(f"{channel.config.name} TX {data.hex()}")

            await client.write_gatt_char(channel.config.write_uuid, data, response=True)

        except Exception as ex:
            logger.error(f"{channel.config.name} write failed {ex}")
            raise ex


async def ble_manager(
    channels: List[ChannelRuntime],
    address: str,
    logging_parameters: Sequence[Optional[LoggingParameters]]
) -> None:

    reconnect_delay: int = 2
    while True:
        try:
            logger.info("Scanning for BLE device...")
            device = await BleakScanner.find_device_by_address(
                address,
                timeout=10,
            )

            if device is None:
                logger.info("Device not found waiting")
                await asyncio.sleep(reconnect_delay)
                continue

            async with BleakClient(device) as client:
                logger.info("BLE connected")
                tasks = []
                for index,channel in enumerate(channels):
                    await client.start_notify(
                        channel.config.notify_uuid,
                        make_notify_handler(channel, logging_parameters[index]),
                    )
                    tasks.append(ble_write_worker(client, channel))
                    logger.info(f"Subscribed {channel.config.name}")

                reconnect_delay = 2
                await asyncio.gather(*tasks, return_exceptions=True)

        except Exception as ex:
            logger.info(f"BLE disconnected/error: {ex}")

        logger.info(f"Reconnecting in {reconnect_delay}s...")

        await asyncio.sleep(reconnect_delay)

        reconnect_delay = min(reconnect_delay * 2, 30)
