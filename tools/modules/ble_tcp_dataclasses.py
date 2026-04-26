from dataclasses import dataclass,field
import asyncio

@dataclass(frozen=True)
class ChannelConfiguration:
    name : str
    notify_uuid:str
    write_uuid:str
    tcp_port:int

@dataclass
class ChannelRuntime:
    config:ChannelConfiguration
    tx_queue:asyncio.Queue[bytes] = field(default_factory=asyncio.Queue)
    clients:set[asyncio.StreamWriter] = field(default_factory=set)

