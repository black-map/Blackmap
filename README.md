# BlackMap 5.1.1 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![Version](https://img.shields.io/badge/Version-5.1.1-success)](#)
[![SYN Engine](https://img.shields.io/badge/SYN%20Engine-v2.0-brightgreen)](#)
[![OS](https://img.shields.io/badge/OS-Linux%20%7C%20macOS-blue)](#)

**A Fully Asynchronous, Masscan-comparable Network Reconnaissance Framework written in Rust.**

BlackMap has evolved from a simple port scanner into a complete reconnaissance platform. Version 5.1.1 introduces a **completely redesigned TCP SYN scan engine (v2.0)** with full packet synthesis, proper state tracking, and guaranteed detection of open/closed/filtered ports. Combined with **native Nmap fingerprint detection engines** and **Advanced Application-Layer Service Probes** - all implemented natively in Rust. This release combines lightning-fast stateless raw sockets (Masscan-style) via `pnet`, deep reconnaissance features like CDN/WAF detection, Subdomain Enumeration, and comprehensive service fingerprinting with global connection pooling.

## 🌟 Key Features

*   **✅ Fixed: Stateless Raw TCP SYN Engine v2.0**: Complete rewrite of the SYN scan engine:
    - **Correct packet synthesis**: Proper Ethernet + IPv4 + TCP headers with checksums
    - **Reliable response detection**: Captures SYN-ACK (open), RST (closed), timeout (filtered)
    - **Port tracking**: Accurate state tracking for every scanned port
    - **Performance**: Scan 65,535 ports in < 2 seconds on local networks
    - **Rate limiting**: Adaptive windowed rate limiting for precise control
    - For detailed technical information, see [SYN_SCAN_ENGINE_v2.md](SYN_SCAN_ENGINE_v2.md)
*   **Native Nmap Fingerprint Detection**: Three advanced detection engines implemented natively in Rust:
    - **Service Database Engine**: O(1) TCP/UDP service lookups using native port mappings
    - **Version Detection Engine**: Async service probes with pattern matching against nmap-service-probes database
    - **OS Fingerprint Engine**: TCP stack profile analysis with multi-test scoring (SEQ, T1-T6) and 65%+ accuracy
*   **Advanced Application-Layer Service Probes**: Deep protocol-specific payload validation and parsing for HTTP, SSH, MySQL, PostgreSQL, Redis, MongoDB, and Docker API.
*   **Global Async Connection Pooling**: Highly scalable concurrency engine utilizing Tokio `JoinSet` and `Semaphore` to distribute tasks globally across massive CIDR ranges instead of bottlenecking per host.
*   **Deep Reconnaissance (CDN & WAF)**: Automatically unmasks if a target is protected by Cloudflare, Akamai, Fastly, CloudFront, Imperva, or AWS WAF.
*   **Subdomain Enumeration**: Built-in concurrent DNS brute-forcing to discover hidden infrastructure.
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

BlackMap 5.1 utilizes subcommands to organize its powerful features: `scan` and `subdomains`.

### Port Scanning & Deep Recon

```bash
# Basic scan prioritizing common ports on a single target
blackmap scan example.com -p 22,80,443

# Example Output:
# PORT      STATE    SERVICE
# 22/tcp    open     ssh
# 80/tcp    open     http
# 443/tcp   open     https

# Stateless Masscan-style Raw Socket sweeping across a massive subnet (requires root)
sudo blackmap scan 10.0.0.0/8 -p 80,443 -s tcp-syn --rate-limit 100000

# Stealthy scan utilizing paranoid timing, decoy IPs, and source port randomization
blackmap scan 192.168.1.0/24 -p- -O -V --paranoid --decoys 192.168.1.5,192.168.1.6 -S 53 -oJ results.json
```

### Subdomain Enumeration

```bash
# Concurrently brute-force subdomains using 50 threads
blackmap subdomains target-company.com -t 50
```

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