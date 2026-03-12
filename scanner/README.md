# Blackmap - Advanced Network Scanner

A high-performance, modular network scanner written in C for Linux, designed to be faster and more efficient than Nmap.

## Architecture

```
scanner/
├── core/              # Core scanning engine
│   ├── engine.cpp
│   ├── scheduler.cpp
│   ├── worker_pool.cpp
│   └── packet_dispatcher.cpp
│
├── network/           # Network packet handling
│   ├── raw_socket.c   # Raw socket management
│   ├── packet_builder.c  # IP/TCP/UDP packet construction
│   ├── checksum.c     # TCP/IP checksum calculation
│   ├── ip_header.c
│   ├── tcp_header.c
│   └── udp_header.c
│
├── scanners/          # Scan type implementations
│   ├── syn_scan.cpp
│   ├── connect_scan.cpp
│   ├── ack_scan.cpp
│   ├── fin_scan.cpp
│   ├── null_scan.cpp
│   ├── xmas_scan.cpp
│   └── udp_scan.cpp
│
├── detection/         # Service & OS detection
│   ├── service_detection.cpp
│   ├── banner_grabber.cpp
│   └── os_fingerprint.cpp
│
├── utils/            # Utility functions
│   ├── cidr_parser.cpp
│   ├── port_parser.cpp
│   ├── timer.cpp
│   └── logger.cpp
│
├── include/          # Header files
│
├── cli/              # Command-line interface
│   ├── main.cpp
│   └── argument_parser.cpp
│
├── scripts/          # Scripting engine
│   └── scripting_engine.cpp
│
└── Makefile
```

## Features

- **Multiple Scan Types**: SYN, CONNECT, ACK, FIN, NULL, XMAS, UDP
- **High Performance**: Multi-threaded with epoll/I/O async
- **Raw Packet Handling**: Manual IP/TCP/UDP header construction
- **Checksum Calculation**: Proper TCP/IP checksum validation
- **Service Detection**: Banner grabbing for 30+ services
- **OS Fingerprinting**: TCP-based OS detection
- **Multiple Output Formats**: Normal, XML, JSON, Grepable

## Compilation

```bash
cd scanner
make
```

## Installation

```bash
sudo make install
```

## Usage

```bash
./blackmap <target> [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-p <ports>` | Ports to scan (e.g., 22,80,443 or 1-1000) |
| `-s <type>` | Scan type: connect, syn, fin, xmas, null, ack, udp |
| `-T <1-5>` | Timing template (T1=slow, T5=fast) |
| `-c <n>` | Concurrent threads |
| `-sV` | Service version detection |
| `-O` | OS detection (requires root) |
| `-oN <file>` | Normal output |
| `-oX <file>` | XML output |
| `-oJ <file>` | JSON output |
| `-oG <file>` | Grepable output |
| `-v` | Verbose mode |

### Examples

```bash
# Basic scan
./blackmap 192.168.1.1 -p 1-1000

# SYN scan with service detection
./blackmap 192.168.1.1 -p 1-1000 -sS -sV

# Fast scan
./blackmap target.com -p 1-10000 -T5

# UDP scan
./blackmap 10.0.0.1 -p 1-1000 -sU
```

## Scan Types Explained

### SYN Scan (-sS)
Sends SYN packet and analyzes response:
- SYN+ACK → PORT_OPEN
- RST → PORT_CLOSED
- No response → PORT_FILTERED

### CONNECT Scan (-sT)
Standard TCP connect scan, no root required.

### FIN/XMAS/NULL Scan
Stealth scans that send packets without SYN flag:
- No response → PORT_OPEN (firewall rule)
- RST → PORT_CLOSED

### ACK Scan (-sA)
Used for firewall detection:
- RST → PORT_UNFILTERED
- No response → PORT_FILTERED

### UDP Scan (-sU)
UDP protocol scanning:
- ICMP Port Unreachable → PORT_CLOSED
- No response → PORT_OPEN|FILTERED

## Output Formats

### Normal
```
22/tcp   OPEN       ssh
80/tcp   OPEN       http
```

### JSON
```json
{
  "scanner": "blackmap",
  "results": [
    {"port": 22, "protocol": "tcp", "state": "OPEN", "service": "ssh"}
  ]
}
```

## Implementation Details

### Raw Socket Handling
The scanner uses raw sockets to craft custom IP/TCP/UDP packets, allowing complete control over packet headers.

### Packet Construction
Manual construction of:
- IPv4 headers with proper checksum
- TCP headers with configurable flags (SYN, FIN, ACK, etc.)
- UDP headers for UDP scanning

### Thread Pool
Multi-threaded architecture using pthread for parallel scanning, with configurable thread count.

## Performance

Blackmap is optimized for high-speed scanning:
- epoll-based I/O multiplexing
- Non-blocking socket operations
- Thread pool for parallel processing
- Configurable rate limiting

## Requirements

- Linux OS
- GCC compiler
- Root privileges (for raw socket scans)
- pthread library

## License

MIT License

## Disclaimer

This tool is for authorized security testing only. Always obtain proper authorization before scanning networks you do not own.
