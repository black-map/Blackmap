# BlackMap 4.0 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![Version](https://img.shields.io/badge/Version-4.0.0-success)](#)
[![OS](https://img.shields.io/badge/OS-Linux%20%7C%20macOS-blue)](#)

**A Fully Asynchronous, Professional-Grade Network Reconnaissance Framework written in Rust.**

BlackMap has evolved from a simple port scanner into a complete reconnaissance platform. Version 4.0 introduces an entirely modular architecture, combining lightning-fast async socket connections (via Tokio) with deep reconnaissance features like CDN/WAF detection, Subdomain Enumeration, and advanced service fingerprinting.

## 🌟 Key Features

*   **Blazing Fast Port Scanning**: Capable of parallel resolving and scanning of millions of targets in seconds utilizing connection pooling.
*   **Intelligent OS Fingerprinting**: Built-in OS heuristics engine utilizing base TTL extraction (without requiring raw sockets or root privileges).
*   **Deep Service Detection**: Advanced banner grabbing combined with implicit Common Ports Fallback (HTTP, HTTPS, SSH arrays natively assigned without active regex matching needing to succeed).
*   **Subdomain Enumeration**: Built-in concurrent DNS brute-forcing to discover hidden infrastructure.
*   **Deep Reconnaissance (CDN & WAF)**: Automatically unmasks if a target is protected by Cloudflare, Akamai, Fastly, CloudFront, Imperva, or AWS WAF.
*   **Ultra Stealth**: Granular dynamic stealth profiles ranging from Level 0 (Aggressive) to Level 5 (Ghostly), with native packet rate-limiting constraints.
*   **Multi-Format Output**: Get your results in Human-readable Tables, JSON, or CSV formats natively.
*   **Distributed Mode**: Native Master/Worker distributed cluster logic to deploy workers across subnets!

## 📦 Installation

To quickly get started:

```bash
git clone https://github.com/Brian-Rojo/Blackmap
cd Blackmap/rust
cargo build --release
sudo cp target/release/blackmap /usr/local/bin/
```
Refer to the [INSTALL.md](INSTALL.md) file for more granular info regarding compilation environments.

## ⚡ Usage Examples

BlackMap 4.0 utilizes subcommands to organize its powerful features: `scan` and `subdomains`.

### Port Scanning & Deep Recon

```bash
# Basic scan prioritizing common ports on a single target
blackmap scan example.com -p 22,80,443

# Example Output:
# PORT      STATE    SERVICE
# 22/tcp    open     ssh
# 80/tcp    open     http
# 443/tcp   open     https

# Stealthy scan, with OS Fingerprinting and Service Detection enabled, saving to JSON
blackmap scan 192.168.1.0/24 -p- -O -V --stealth 3 -oJ results.json

# Launch an insanely fast scan with a maximum packet rate limit
blackmap scan 10.0.0.0/8 -p 80,443 --timing insane --rate-limit 10000
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

The v4.0 update migrated BlackMap into a heavily modular, Rust-first design pattern. Legacy C bindings were minimized and isolated. The framework is strictly built around `tokio` for scheduling. For more information, please see [ARCHITECTURE.md](ARCHITECTURE.md).

## 🤝 Roadmap & Open Source Community

This project was redesigned entirely around the Open Source community ethos. We encourage developers to experiment with writing rust-based plugins and expanding the JSON fingerprint DB. Please check our [ROADMAP.md](ROADMAP.md) for our goals through v4.5+.

## 📊 Project Statistics & Comparisons

Currently, BlackMap 4.0 sits at roughly **~4,000 lines of code** bridging pure Safe Rust async logic with ultra-fast legacy C engines.

### BlackMap vs Nmap vs RustScan
*   **Nmap**: The industry standard. Extremely feature-rich (NSE scripting, raw packet manipulation) but notoriously slow for scanning massive class A/B public networks due to legacy socket handling limits.
*   **RustScan**: A phenomenal wrapper that port scans in seconds via Rust logic, but ultimately pipes open ports *back into Nmap* for service detection—making it heavily dependent on Nmap being installed locally.
*   **BlackMap**: Acts as the ultimate bridge. It delivers RustScan's insane asynchronous connection speeds directly baked in natively, with its **own** native service detection, banner grabbing, CDN/WAF unmasking, and Ping-based OS fingerprinting. Complete autonomy; **zero dependency on Nmap binaries**.