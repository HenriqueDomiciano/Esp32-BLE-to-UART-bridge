import asyncio
import logging
import time
from typing import Optional

from modules.pcap_logger.pcap_logger_dataclasses import LoggingParameters
from scapy.all import IP, UDP, Raw, wrpcap

logger = logging.getLogger(__name__)

buffer = []

async def pcap_logger(logging_parameters: Optional[LoggingParameters]):
    last_flush = time.time()
    logger.info("[LOG] Starting logging listening")
    if logging_parameters is None:
        return
    try:
        while True:
            try:
                if logging_parameters is not None:
                    item = await asyncio.wait_for(
                        logging_parameters.logging_queue.get(), timeout=1
                    )
                    if item is None:
                        break
                    pkt = (
                        (IP(dst=item.destination, src=item.src))
                        / UDP(sport=logging_parameters.port, dport=logging_parameters.port)
                        / Raw(load=item.data)
                    )
                    buffer.append(pkt)
                    logging_parameters.logging_queue.task_done()
            except asyncio.TimeoutError:
                pass

            if buffer != [] and len(buffer)>=10 or ((time.time()-last_flush) > 1):
                if buffer != []:
                    await asyncio.to_thread(wrpcap, logging_parameters.pcap_file_name, buffer, append=True)
                    buffer.clear()
                    last_flush = time.time()
    except asyncio.CancelledError:
        pass
    finally:
        if buffer:
            wrpcap(logging_parameters.pcap_file_name,buffer,append=True)
        logger.info("[LOG] Finishing logging")