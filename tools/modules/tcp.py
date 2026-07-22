from typing import Optional
from modules.ble_tcp_dataclasses import ChannelRuntime
import asyncio
import logging
from modules.pcap_logger.pcap_logger_dataclasses import LoggingParameters, PacketInfo

logger = logging.getLogger(__name__)


async def forward_to_tcp(
    channel: ChannelRuntime,
    payload: bytes,
    logging_parameters: Optional[LoggingParameters]
) -> None:

    dead_clients: list[asyncio.StreamWriter] = []

    for writer in channel.clients:
        try:
            writer.write(payload)
            if logging_parameters is not None:
                packet = PacketInfo(payload,logging_parameters.destination_ip,logging_parameters.source_ip)
                await logging_parameters.logging_queue.put(packet)
            logger.info(f"Sending {len(payload)} to {len(channel.clients)}")
            await writer.drain()
        except Exception:
            dead_clients.append(writer)

    for client in dead_clients:
        channel.clients.discard(client)


async def handle_tcp_client(
    channel: ChannelRuntime,
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    logging_parameters: Optional[LoggingParameters]
) -> None:

    channel.clients.add(writer)

    addr = writer.get_extra_info("peername")
    logger.info(f"{channel.config.name}: client connected {addr}")

    try:
        while True:
            data: bytes = await reader.read(4096)

            logger.info(f"TCP RX {data.hex()}")
            
            if logging_parameters is not None:
                packet = PacketInfo(data,src=logging_parameters.source_ip,destination=logging_parameters.source_ip)
                await logging_parameters.logging_queue.put(packet)

            if not data:
                break

            await channel.tx_queue.put(data)

    except Exception as ex:
        logger.warning(f"{channel.config.name}: client error {ex}")

    finally:
        channel.clients.discard(writer)
        writer.close()
        await writer.wait_closed()

        logger.info(f"{channel.config.name}: client disconnected")


async def start_channel_server(
    channel: ChannelRuntime,
    logging_channel: Optional[LoggingParameters]
) -> None:

    server = await asyncio.start_server(
        lambda r, w:
            handle_tcp_client(channel, r, w, logging_channel),
        host="127.0.0.1",
        port=channel.config.tcp_port,
    )

    logger.info(
        f"{channel.config.name} TCP listening "
        f"on {channel.config.tcp_port}"
    )

    async with server:
        await server.serve_forever()