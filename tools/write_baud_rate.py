import asyncio
import enum
from modules.constants import ADDRESS
import logging 
from bleak import BleakClient,BleakScanner
import argparse


logger = logging.getLogger(__name__)

UART_BAUD_RATE_1 = "0000abf3-0000-1000-8000-00805f9b34fb"
UART_BAUD_RATE_0 = "0000abe3-0000-1000-8000-00805f9b34fb"

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="BLE TCP Bridge"
    )

    parser.add_argument(
        "--uart",
        choices=["uart1","uart0","both"],
        default="both",
        help="Select UART to update"
    )

    parser.add_argument(
        "--baud_rate",
        type=int,
        choices=[4800,9600, 115200, 19200, 38400, 57600], default=115200, 
        help="Select the baud rate"
    )
    parser.add_argument("--mac",type=str,default=ADDRESS,help="The MAC address of the device")

    return parser.parse_args()

async def write_baud_rate(baud_rate:int,uart:str,address:str):
    device = await BleakScanner.find_device_by_address(address,timeout=10)

    if device is None:
        logger.info("Device not found !!!")
        exit(1)

    async with BleakClient(device) as client:
        if client.is_connected:
            if uart == "uart1":
                await client.write_gatt_char(UART_BAUD_RATE_1,baud_rate.to_bytes(4,"little"))
            elif uart == "uart0":
                await client.write_gatt_char(UART_BAUD_RATE_0,baud_rate.to_bytes(4,"little"))
            elif uart== "both":
                await client.write_gatt_char(UART_BAUD_RATE_0,baud_rate.to_bytes(4,"little"))
                await client.write_gatt_char(UART_BAUD_RATE_1,baud_rate.to_bytes(4,"little"))
            else:
                logger.warning(f"{uart} not supported")
                return

if __name__ == "__main__":
    args = parse_args()
    asyncio.run(write_baud_rate(args.baud_rate,args.uart,args.mac))