# BlackMap 5.1 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![Version](https://img.shields.io/badge/Version-5.1.0-success)](#)
[![OS](https://img.shields.io/badge/OS-Linux%20%7C%20macOS-blue)](#)

**A Fully Asynchronous, Masscan-comparable Network Reconnaissance Framework written in Rust.**

BlackMap has evolved from a simple port scanner into a complete reconnaissance platform. Version 5.1 introduces an entirely modular Cargo Workspace architecture, combining lightning-fast stateless raw sockets (Masscan-style) via `pnet`, deep reconnaissance features like CDN/WAF detection, Subdomain Enumeration, and advanced service fingerprinting.

## 🌟 Key Features

*   **Stateless Raw Socket Engine**: Scan **65,535 ports in < 2 seconds** utilizing a Masscan-style raw packet generator built on `pnet` and independent background kernel receptors.
*   **Intelligent OS Fingerprinting**: Built-in OS heuristics engine utilizing base TTL extraction (without requiring raw sockets or root privileges).
*   **Deep Service Detection**: Advanced banner grabbing combined with implicit Common Ports Fallback (HTTP, HTTPS, SSH arrays natively assigned without active regex matching needing to succeed).
*   **Subdomain Enumeration**: Built-in concurrent DNS brute-forcing to discover hidden infrastructure.
*   **Deep Reconnaissance (CDN & WAF)**: Automatically unmasks if a target is protected by Cloudflare, Akamai, Fastly, CloudFront, Imperva, or AWS WAF.
*   **Ultra Stealth & Evasion**: Granular dynamic stealth profiles ranging from Level 0 (Aggressive) to Level 5 (Paranoid), Native packet rate-limiting, Decoy IP spoofing, TCP Option Jitter, and Source Port randomization.
*   **Multi-Format Output**: Get your results in Human-readable Tables, JSON, or CSV formats natively.
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

### Distributed Scan Architecture (Clustering)

```bash
# Node 1: Start Master API server
blackmap scan target.com --master 0.0.0.0:8000

# Node 2: Hook as worker 
blackmap scan --worker 192.168.1.50:8000
```

## 🏗️ Architecture

The v5.1 update migrated BlackMap into a heavily modular Cargo Workspace design pattern. Legacy C bindings were isolated into the `core/` boundary, while specialized logic like `modules`, `stealth`, and the `raw_scanner` pnet engine run concurrently as internal detached libraries. For more information, please see [ARCHITECTURE.md](ARCHITECTURE.md).

## 🤝 Roadmap & Open Source Community

This project was redesigned entirely around the Open Source community ethos. We encourage developers to experiment with writing rust-based plugins and expanding the JSON fingerprint DB. Please check our [ROADMAP.md](ROADMAP.md) for our goals through v5.5+.

## 📊 Project Statistics & Comparisons

Currently, BlackMap 5.1 sits at **10,886 lines of code**, bridging pure Safe Rust async logic with ultra-fast legacy C engines and a massive Cargo Workspace footprint.

### BlackMap vs Nmap vs Masscan vs RustScan
*   **Masscan**: The king of speeds, but lacks native deep validation. Only builds raw IP strings without complex Service detection layers attached post-scan.
*   **Nmap**: The industry standard. Extremely feature-rich (NSE scripting, raw packet manipulation) but notoriously slow for scanning massive class A/B public networks due to legacy sequential looping patterns.
*   **RustScan**: A phenomenal wrapper that port scans in seconds via Rust logic, but ultimately pipes open ports *back into Nmap* for service detection—making it heavily dependent on Nmap being installed locally.
*   **BlackMap**: Acts as the ultimate bridge. It delivers **Masscan's raw socket speeds natively via pnet**, with its **own** intrinsic Rust-native service detection, banner grabbing, CDN/WAF unmasking, and Ping-based OS fingerprinting. Complete autonomy; **zero dependency on external Nmap/Masscan binaries**.