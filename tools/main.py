import argparse
from typing import List
from modules.ble_tcp_dataclasses import ChannelConfiguration, ChannelRuntime
import asyncio
from modules.constants import ADDRESS
from modules.tcp import start_channel_server
from modules.ble import ble_manager
import logging



def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="BLE TCP Bridge"
    )

    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug logging"
    )

    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        default="INFO",
        help="Set logging level"
    )
    parser.add_argument(
        "--tcp-port-uart-0", 
        type=int,
        default=2222        
    )
    parser.add_argument(
        "--tcp-port-uart-1", 
        type=int,
        default=2223        
    )
    parser.add_argument(
        "--mac",
        type=str,
        default=ADDRESS
    )

    return parser.parse_args()

def setup_logging(level: str) -> None:
    logging.basicConfig(
        level=getattr(logging, level),
        format=(
            "%(asctime)s "
            "%(levelname)s "
            "%(name)s: %(message)s"
        ),
    )


async def main():

    args = parse_args()
    setup_logging(args.log_level)

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

    tcp_tasks = [
        asyncio.create_task(
            start_channel_server(ch)
        )
        for ch in channel_runtime
    ]

    ble_task = asyncio.create_task(
        ble_manager(channel_runtime,args.mac)
    )

    await asyncio.gather(
        ble_task,
        *tcp_tasks
    )



if __name__ == "__main__":
    asyncio.run(main())
