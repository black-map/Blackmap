# Blackmap

Advanced Network Scanner - A high-performance port scanner with service detection, OS fingerprinting, and multiple scan techniques.

## Features

- **Multiple Scan Types**: SYN, FIN, XMAS, NULL, ACK, UDP, CONNECT
- **Service Detection**: Identifies 30+ services with version detection
- **OS Fingerprinting**: Advanced TCP-based OS detection
- **High Performance**: Multi-threaded scanning up to 500 threads
- **Timing Templates**: T1-T5 for speed/stealth balance
- **Multiple Output Formats**: Normal, XML, JSON, Grepable (Nmap-compatible)
- **Evasive Techniques**: Decoy IPs, random delays, source port spoofing

## Installation

```bash
gcc -o blackmap blackmap.c -lpthread
```

## Usage

```bash
./blackmap <target> [options]
```

### Options

| Flag | Description |
|------|-------------|
| `-p <ports>` | Ports to scan (e.g., 22,80,443 or 1-1000 or all) |
| `-s <type>` | Scan type: connect, syn, fin, xmas, null, udp, ack |
| `-T <1-5>` | Timing template (T1=slow, T5=fast) |
| `-c <n>` | Concurrent threads (default: 50, max: 500) |
| `-sV` | Service version detection |
| `-O` | OS detection (requires root) |
| `-A` | Enable all detections (OS + version) |
| `-oN <file>` | Normal output |
| `-oX <file>` | XML output |
| `-oJ <file>` | JSON output |
| `-oG <file>` | Grepable output |
| `-v` | Verbose mode |
| `-vv` | Very verbose |

### Examples

```bash
# Basic scan
./blackmap 192.168.1.1 -p 1-1000

# Fast scan with version detection
./blackmap target.com -p 1-10000 -sS -sV -T5

# Stealth scan with OS detection
./blackmap 10.0.0.1 -p 1-1000 -sS -O -T2

# Scan with output files
./blackmap 192.168.1.1 -p 22,80,443 -sV -oN scan.txt -oJ scan.json
```

## Supported Services

Blackmap detects the following services:

- FTP (21)
- SSH (22)
- Telnet (23)
- SMTP (25, 465, 587)
- DNS (53)
- HTTP (80, 8080, 8443)
- POP3 (110, 995)
- IMAP (143, 993)
- HTTPS (443)
- SMB (445)
- MSSQL (1433)
- MySQL (3306)
- PostgreSQL (5432)
- RDP (3389)
- VNC (5900)
- Redis (6379)
- MongoDB (27017)
- Elasticsearch (9200)
- And more...

## Scan Types

| Type | Description | Requires Root |
|------|-------------|---------------|
| CONNECT | Standard TCP connection | No |
| SYN | SYN scan (half-open) | Yes |
| FIN | FIN scan (stealth) | Yes |
| XMAS | XMAS scan (stealth) | Yes |
| NULL | NULL scan (stealth) | Yes |
| ACK | ACK scan (firewall detection) | Yes |
| UDP | UDP scan | Yes |

## Timing Templates

| Template | Timeout | Threads | Delay | Use Case |
|----------|---------|---------|-------|----------|
| T1 | 30000ms | 5 | 15000ms | Very slow/stealth |
| T2 | 15000ms | 10 | 5000ms | Slow |
| T3 | 8000ms | 50 | 1000ms | Normal (default) |
| T4 | 4000ms | 150 | 250ms | Fast |
| T5 | 2000ms | 300 | 0ms | Very fast |

## Output Formats

### Normal Output
```
[OPEN] 22/tcp ssh
[OPEN] 80/tcp http
```

### Verbose (Nmap-style)
```
Nmap scan report for 192.168.1.1
Host is up (0.0023s latency).

PORT      STATE SERVICE       VERSION
22/tcp    open  ssh          OpenSSH 8.0
80/tcp    open  http         Apache 2.4.41
```

### JSON
```json
{
  "scanner": "blackmap",
  "version": "1.2",
  "results": [
    {"port": 22, "protocol": "tcp", "state": "OPEN", "service": "ssh"}
  ]
}
```

## Performance

Blackmap is optimized for speed:
- Multi-threaded architecture (up to 500 threads)
- Non-blocking I/O for connect scans
- Efficient memory management
- Optimized for modern Linux kernels

## License

MIT License

## Disclaimer

This tool is intended for authorized security testing only. Always obtain proper authorization before scanning any network you do not own.
