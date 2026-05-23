import argparse
import datetime
from typing import List
from modules import pcap_logger
from modules.ble_tcp_dataclasses import ChannelConfiguration, ChannelRuntime
import asyncio
from modules.constants import ADDRESS
from modules.pcap_logger.pcap_logger_dataclasses import LoggingParameters
from modules.tcp import start_channel_server
from modules.ble import BleManager
from modules.pcap_logger.pcap_logger import pcap_logger
import logging
from datetime import datetime


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="BLE TCP Bridge")

    parser.add_argument("--debug", action="store_true", help="Enable debug logging")

    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        default="INFO",
        help="Set logging level",
    )
    parser.add_argument("--tcp-port-uart-0", type=int, default=2222)
    parser.add_argument("--tcp-port-uart-1", type=int, default=2223)
    parser.add_argument("--mac", type=str, default=ADDRESS)
    parser.add_argument("--log-pcap", action="store_true", default=False)

    return parser.parse_args()


def setup_logging(level: str) -> None:
    logging.basicConfig(
        level=getattr(logging, level),
        format=("%(asctime)s %(levelname)s %(name)s: %(message)s"),
    )


async def main():

    args = parse_args()
    setup_logging(args.log_level)
    date_str = datetime.now().strftime("%d-%m-%Y-%H-%M-%S")
    channels: List[ChannelConfiguration] = [
        ChannelConfiguration(
            name="uart1",
            notify_uuid="0000abf2-0000-1000-8000-00805f9b34fb",
            write_uuid="0000abf1-0000-1000-8000-00805f9b34fb",
            tcp_port=args.tcp_port_uart_1,
        ),
        ChannelConfiguration(
            name="uart0",
            notify_uuid="0000abe2-0000-1000-8000-00805f9b34fb",
            write_uuid="0000abe1-0000-1000-8000-00805f9b34fb",
            tcp_port=args.tcp_port_uart_0,
        ),
    ]
    channel_runtime: List[ChannelRuntime] = [ChannelRuntime(ch) for ch in channels]
    final_channels = []
    if args.log_pcap:
        for index, channel in enumerate(channel_runtime):
            logger_value = LoggingParameters(
                source_ip=f"10.0.0.{(index + 1) * 2}",
                destination_ip=f"10.0.0.{(index + 1) * 2 + 1}",
                pcap_file_name=f"{(index + 1) * 2}-{(index + 1) * 2 + 1}-{date_str}.pcap",
            )
            final_channels.append((channel, logger_value))
    else:
        final_channels = [(ch, None) for ch in channel_runtime]

    tcp_tasks = [
        asyncio.create_task(start_channel_server(ch, lg)) for ch, lg in final_channels
    ]
    final_logs = [logs[1] for logs in final_channels]
    ble_manager_object = BleManager(args.mac, channel_runtime, final_logs)
    ble_task = asyncio.create_task(ble_manager_object.ble_manager())

    if args.log_pcap is not None or args.log_pcap:
        pcaps_tasks = [asyncio.create_task(pcap_logger(log)) for log in final_logs]
        await asyncio.gather(ble_task, *tcp_tasks, *pcaps_tasks)
    else:
        await asyncio.gather(
            ble_task,
            *tcp_tasks,
        )


if __name__ == "__main__":
    asyncio.run(main())
