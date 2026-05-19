
from asyncio import Queue
from dataclasses import dataclass, field
from datetime import datetime

@dataclass
class PacketInfo:
    data:bytes
    src:str
    destination:str

@dataclass
class LoggingParameters: 
    logging_queue:Queue[PacketInfo] = field(default_factory=Queue)
    port : int = 4096
    source_ip:str = "10.0.0.2"
    destination_ip:str = "10.0.0.3"
    pcap_file_name:str = f"writing_log_{'datetime.now():%Y-%m-%d-%H-%M-%S'}.pcap"


