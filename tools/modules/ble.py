import asyncio
from typing import List, Callable, Optional, Sequence

from bleak.backends import client
from modules.ble_tcp_dataclasses import ChannelRuntime
import logging
from bleak import BleakClient, BleakScanner
from modules.pcap_logger.pcap_logger_dataclasses import LoggingParameters
from modules.tcp import forward_to_tcp
from bleak.backends.characteristic import (
    BleakGATTCharacteristic,
)

logger = logging.getLogger(__name__)


class BleManager:
    def __init__(self, address: str,channels: List[ChannelRuntime], logging_parameters: Sequence[Optional[LoggingParameters]]): 
        self.tasks = []
        self.address = address
        self.running = True
        self.device = None
        self.client = None
        self.logging_parameters = logging_parameters
        self.channels = channels
        self.disconnected_event = asyncio.Event()

    def make_notify_handler(
        self,channel,logging_parameter
    ) -> Callable[[BleakGATTCharacteristic, bytearray], None]:
        def handler(_: BleakGATTCharacteristic, data: bytearray):
            logger.info(f"{channel.config.name} RX {bytes(data).hex()}")
            asyncio.create_task(
                forward_to_tcp(channel, bytes(data), logging_parameter)
            )

        return handler

    async def ble_write_worker(self,client:BleakClient, channel: ChannelRuntime):
        if client is not None:
            while client.is_connected:
                data = await channel.tx_queue.get()

                try:
                    logger.info(f"{channel.config.name} TX {data.hex().upper()}")

                    await client.write_gatt_char(
                        channel.config.write_uuid, data, response=True
                    )

                except Exception as ex:
                    logger.error(f"{channel.config.name} write failed {ex}")
                    raise ex

    async def cleanup_tasks(self):
        for task in self.tasks:
            task.cancel()
        await asyncio.gather(
               *self.tasks,
               return_exceptions=True,
           )       
        self.tasks.clear()

    async def cleanup_client(self):
        if self.client:
            try:
                await self.client.disconnect()
            except Exception:
                pass
            self.client = None
            self.device = None


    def on_disconnect(self, client):
        logger.info("BLE Client disconnected reconnecting")
        self.disconnected_event.set()
    
    async def connect(self):
        logger.info("Scanning for BLE device...")
        while True:
            self.device = await BleakScanner.find_device_by_address(
                self.address, timeout=10
            )
            if self.device is None:
                logger.info("Device not found waiting")
                await asyncio.sleep(2)
                continue
            else:
                self.client = BleakClient(
                    self.address, disconnected_callback=self.on_disconnect
                )
                await self.client.connect()
                return

    async def ble_manager(self):

        while True:

            try:
                self.disconnected_event = asyncio.Event()

                await self.connect()

                logger.info("BLE connected")

                self.tasks = []

                client = self.client

                if client is None: 
                    await asyncio.sleep(2)
                    continue

                for index, channel in enumerate(self.channels):

                    await client.start_notify(
                        channel.config.notify_uuid,
                        self.make_notify_handler(
                            channel,
                            self.logging_parameters[index],
                        ),
                    )

                    self.tasks.append(
                        asyncio.create_task(
                            self.ble_write_worker(client, channel)
                        )
                    )
                    logger.info(f"Subscribed {channel.config.name}")

                await self.disconnected_event.wait()

            except Exception as ex:
                logger.exception(ex)

            finally:
                await self.cleanup_tasks()
                await self.cleanup_client()

            logger.info("Reconnecting in 2 seconds")
            await asyncio.sleep(2)