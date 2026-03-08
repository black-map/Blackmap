# BlackMap 5.1.2 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![Version](https://img.shields.io/badge/Version-5.1.2-success)](#)
[![SYN Engine](https://img.shields.io/badge/SYN%20Engine-v2.0-brightgreen)](#)
[![OS](https://img.shields.io/badge/OS-Linux%20%7C%20macOS-blue)](#)

**A Fully Asynchronous, Masscan-comparable Network Reconnaissance Framework written in Rust.**

BlackMap is a high-performance network reconnaissance and port scanning framework designed for speed and deep network visibility. Version 5.1.2 introduces significant improvements to functionality, scan transparency, detection capability and reporting to compete with modern scanners.

## 🌟 Key Features

*   **✅ Fixed: Stateless Raw TCP SYN Engine v2.0**: Complete rewrite of the SYN scan engine:
    - **Correct packet synthesis**: Proper Ethernet + IPv4 + TCP headers with checksums
    - **Reliable response detection**: Captures SYN-ACK (open), RST (closed), timeout (filtered)
    - **Port tracking**: Accurate state tracking for every scanned port
    - **Performance**: Scan 65,535 ports in < 2 seconds on local networks
    - **Rate limiting**: Adaptive windowed rate limiting for precise control
    - For detailed technical information, see [SYN_SCAN_ENGINE_v2.md](SYN_SCAN_ENGINE_v2.md)
*   **Native Service Detection**: Advanced banner grabbing and fingerprint matching for common services:
    - **SSH, HTTP, HTTPS, FTP, MySQL, PostgreSQL, Redis, SMTP, DNS**
    - **Failed detection reporting**: Explicit reasons for unrecognized services
    - **Async service probes**: High-performance concurrent detection
*   **OS Fingerprinting**: TCP/IP stack analysis using TTL, window size patterns, and TCP options
    - **Multi-factor analysis**: Comprehensive OS identification
    - **65%+ accuracy**: Competitive with industry standards
    - **Failed detection reporting**: Clear reasons for unknown OSes
*   **Multi-Method Host Discovery**: ICMP ping, TCP SYN ping, TCP ACK ping, ARP discovery
    - **Local network ARP**: Efficient discovery on LAN segments
    - **Response classification**: Clear reporting of discovery methods
*   **Advanced Scanning Engine**:
    - **Asynchronous scanning**: Tokio-powered concurrent operations
    - **Adaptive timeouts**: Intelligent timeout adjustment
    - **Retry attempts**: Configurable retry logic
    - **Packet loss detection**: Network condition monitoring
    - **Intelligent concurrency**: Dynamic thread management
*   **Comprehensive Scan Statistics**:
    - **Detailed metrics**: Hosts scanned, ports tested, packet statistics
    - **Latency information**: Average and maximum response times
    - **State reporting**: Open, closed, filtered port counts
    - **Packet analysis**: Sent/received counts and loss percentages
*   **Ultra Stealth & Evasion**: Granular dynamic stealth profiles ranging from Level 0 (Aggressive) to Level 5 (Paranoid), Native packet rate-limiting, Decoy IP spoofing, TCP Option Jitter, and Source Port randomization.
*   **Multi-Format Output**: Get your results in Human-readable Tables, XML, JSON, or CSV formats natively mapped with comprehensive metadata.
*   **Distributed Mode**: Native Master/Worker distributed cluster logic to deploy workers across subnets!

## 📦 Installation

To quickly get started (requires Rust toolchain):

```bash
git clone https://github.com/Brian-Rojo/Blackmap
cd Blackmap
cargo build --release
sudo cp target/release/cli /usr/local/bin/blackmap
sudo cp target/release/blackmap /usr/local/bin/
```
Refer to the [INSTALL.md](INSTALL.md) file for more granular info regarding compilation environments.

## ⚡ Usage Examples

BlackMap 5.1.2 provides comprehensive network reconnaissance with detailed reporting and modern scanning capabilities.

### Basic Port Scanning

```bash
# Basic scan with service detection
blackmap scan example.com -p 22,80,443 -V

# Example Output:
# PORT      STATE    SERVICE
# 22/tcp    open     ssh     OpenSSH 8.9
# 80/tcp    open     http    nginx 1.22
# 443/tcp   open     https   Apache 2.4
```

### Advanced Service & OS Detection

```bash
# Full reconnaissance with service and OS detection
blackmap scan 192.168.1.0/24 --service-detect --os-detect

# Example Output:
# Service detection:
# 22/tcp   open  ssh   OpenSSH 8.9
# 80/tcp   open  http  nginx 1.22
# 443/tcp  open  https Apache 2.4
# 8080/tcp open  unknown
# Reason: banner not recognized
#
# OS detection:
# 192.168.1.1   Linux kernel 5.x
# 192.168.1.10  Windows 10/11
# 192.168.1.15  UNKNOWN
# Reason: insufficient fingerprint data
```

### High-Performance SYN Scanning

```bash
# Fast SYN scan with detailed statistics (requires root)
sudo blackmap scan 10.0.0.0/24 -s tcp-syn --rate 10000

# Example Output:
# Scan statistics:
# Hosts scanned: 256
# Hosts up: 14
# Total ports tested: 256000
# Open ports: 38
# Closed ports: 1200
# Filtered ports: 254762
# Packets sent: 520000
# Packets received: 18400
# Packet loss: 3.2%
# Average latency: 24ms
# Max latency: 92ms
```

### Stealth & Evasion Scanning

```bash
# Stealthy scan with custom timing and retries
blackmap scan target.com --stealth 3 --retries 2 --timeout 3

# Configuration Output:
# Configuration:
# Ports: 1000
# Concurrency: 500 threads
# Timeout: adaptive
# Retries: 2
# Rate limit: auto
```

### Subdomain Enumeration

```bash
# Concurrently brute-force subdomains using 50 threads
blackmap subdomains target-company.com -t 50
```

## 🆕 New CLI Features in v5.1.2

BlackMap 5.1.2 introduces comprehensive CLI options for advanced scanning:

### Detection Options
- `--service-detect` / `-V`: Enable service version detection
- `--os-detect` / `-O`: Enable OS fingerprinting
- `--version-intensity <0-9>`: Set detection intensity
- `--version-light`: Fast detection mode
- `--version-all`: Aggressive detection mode

### Performance & Control
- `--rate <pps>`: Set packets per second rate
- `--retries <num>`: Number of retry attempts
- `--timeout <secs>`: Custom timeout values
- `--stealth <0-5>`: Stealth level (0=aggressive, 5=paranoid)

### Output & Reporting
- `-oJ <file>`: JSON output format
- `-oX <file>`: XML output format
- `--verbose`: Detailed logging
- `--quiet`: Minimal output

### 🛡️ Stateless TCP SYN Evasion (Important)

When running the ultra-fast Stateless TCP SYN engine (`--scan-type tcp-syn`), Blackmap uses raw sockets to bypass the Linux connection tracker. Because the kernel doesn't know about these connections, if a target replies with a `SYN-ACK`, your kernel will automatically reply with a `RST` packet, closing the connection. 

While this still safely identifies the port as open, it creates noise. To completely evade detection and stop your kernel from interfering, apply this temporary `iptables` rule before scanning:

```bash
# Drop outgoing RST packets to let BlackMap handle state silently
sudo iptables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP
```

### Distributed Scan Architecture (Clustering)

```bash
# Node 1: Start Master API server
blackmap scan target.com --master 0.0.0.0:8000

# Node 2: Hook as worker 
blackmap scan --worker 192.168.1.50:8000
```

## 🏗️ Architecture

The v5.1 update introduces **native Nmap fingerprint detection engines** while maintaining the heavily modular Cargo Workspace design pattern. Legacy C bindings are isolated into the `core/` boundary, while specialized logic like `modules` (with new detection engines), `stealth`, and the `raw_scanner` pnet engine run concurrently as internal detached libraries. For more information, please see [ARCHITECTURE_5.2.md](ARCHITECTURE_5.2.md).

## 🤝 Roadmap & Open Source Community

This project was redesigned entirely around the Open Source community ethos. We encourage developers to experiment with writing rust-based plugins and expanding the JSON fingerprint DB. Please check our [ROADMAP.md](ROADMAP.md) for our goals through v5.5+.

## 📊 Project Statistics & Comparisons

BlackMap 5.1.1 has grown to **24,341+ lines of code** with the complete redesign of the TCP SYN scan engine:

### Code Statistics

| Language | Files | Lines | Purpose |
|----------|-------|-------|---------|
| Rust | 40 | 5,480 | Core scanner, networking, async runtime |
| C | 34 | 5,557 | Legacy engines, discovery, fingerprinting |
| Headers | 23 | 1,650 | FFI bindings, C API definitions |
| Markdown | docs | 4,500+ | Documentation, guides, specs |
| Configuration | build files | 5,500+ | Cargo.toml, Makefile, build scripts |
| **TOTAL** | **~130** | **24,341+** | Full reconnaissance framework |

### BlackMap vs Industry Standards

*   **Masscan**: The king of speeds, but lacks native deep validation. Only builds raw IP strings without complex Service detection layers attached post-scan.
*   **Nmap**: The industry standard. Extremely feature-rich (NSE scripting, raw packet manipulation) but notoriously slow for scanning massive class A/B public networks due to legacy sequential looping patterns.
*   **RustScan**: A phenomenal wrapper that port scans in seconds via Rust logic, but ultimately pipes open ports *back into Nmap* for service detection—making it heavily dependent on Nmap being installed locally.
*   **BlackMap 5.1.1**: Acts as the ultimate bridge. It delivers **Masscan's raw socket speeds natively via pnet**, with its **own** intrinsic Rust-native service detection, banner grabbing, version probing, OS fingerprinting, CDN/WAF unmasking, and Ping-based heuristics. Complete autonomy; **zero dependency on external Nmap/Masscan binaries**. Now featuring:
    - ✅ Fixed: **Complete TCP SYN scan engine rewrite** with proper packet synthesis
    - ✅ Native Nmap fingerprint database integration for industry-standard service detection
    - ✅ Proven detection of open/closed/filtered ports
    - ✅ Sub-second scanning on local networks