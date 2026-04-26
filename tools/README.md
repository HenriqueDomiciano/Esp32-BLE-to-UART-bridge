# BLE TCP Bridge Tools

Python tools that connect to the ESP32 BLE bridge and expose each BLE UART channel as local TCP sockets.

## Features

- Automatic BLE reconnect
- One TCP socket per UART channel
- BLE notifications forwarded to TCP clients
- TCP writes forwarded to BLE
- Multiple TCP clients per channel
- Modular channel configuration

---

# Architecture

```text
TCP Client
   |
localhost socket
   |
Python Bridge
   |
BLE Link
   |
ESP32 Dual UART Bridge
```

Each UART channel is exposed as its own TCP endpoint.

---

# Configuration

Edit:

```text
configuration.py
```

Configure:

- BLE MAC address
- Channel definitions
- TCP listening ports

Example:

```python
ADDRESS = "AA:BB:CC:DD:EE:FF"

CHANNELS = [
    {
        "name": "UART0",
        "tcp_port": 7000,
    },
    {
        "name": "UART1",
        "tcp_port": 7001,
    }
]
```

Service and characteristic UUID mappings are handled internally by the tool modules.

---

# Installation

Install dependencies:

```bash
pip install -r requirements.txt
```

---

# Running

Start bridge:

```bash
python main.py --log-level {DEBUG,INFO,WARNING,ERROR}
```

Expected output:

```text
Scanning for BLE device...
BLE connected
Subscribed UART0
Subscribed UART1

UART0 TCP listening on 2223
UART1 TCP listening on 2222
```

---

# Usage

Connect to UART0:

```bash
nc localhost 2223
```

Connect to UART1:

```bash
nc localhost 2222
```

---

## Data Flow

Sending data through TCP:

```text
TCP -> Python bridge -> BLE -> UART
```

Receiving data from serial device:

```text
UART -> BLE notify -> Python bridge -> TCP
```

---

# Reconnection Behavior

If BLE disconnects:

- Automatic reconnect attempts
- Exponential backoff
- Automatic notification re-subscribe
- TCP servers stay alive

No need to restart TCP clients.

---

# Project Layout

```text
tools/
├── main.py
├── configuration.py
├── requirements.txt
└── modules/
    ├── ble.py
    ├── tcp.py
    ├── constants.py
    └── ble_tcp_dataclasses.py
```
---

# Example Test

With a loopback 

Open a TCP client:

```bash
nc localhost 2223
```

Send:

```text
hello
```

Expected path:

```text
TCP -> BLE -> UART device
```

If device echoes:

```text
UART -> BLE -> TCP
```

you should receive:

```text
hello
```

---

# Future Improvements

- RFC2217 serial server mode
- Multi-device support
- Performance metrics

---

# License

MIT