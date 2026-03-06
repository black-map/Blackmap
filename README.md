# BlackMap 4.0 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![Version](https://img.shields.io/badge/Version-4.0.0-success)](#)
[![OS](https://img.shields.io/badge/OS-Linux%20%7C%20macOS-blue)](#)

**A Fully Asynchronous, Professional-Grade Network Reconnaissance Framework written in Rust.**

BlackMap has evolved from a simple port scanner into a complete reconnaissance platform. Version 4.0 introduces an entirely modular architecture, combining lightning-fast async socket connections (via Tokio) with deep reconnaissance features like CDN/WAF detection, Subdomain Enumeration, and advanced service fingerprinting.

## 🌟 Key Features

*   **Blazing Fast Port Scanning**: Capable of parallel resolving and scanning of millions of targets in seconds utilizing connection pooling.
*   **Subdomain Enumeration**: Built-in concurrent DNS brute-forcing to discover hidden infrastructure.
*   **Deep Reconnaissance (CDN & WAF)**: Automatically unmasks if a target is protected by Cloudflare, Akamai, Fastly, CloudFront, Imperva, or AWS WAF.
*   **Advanced Banner Grabbing**: Connects to open ports to extract service versions and HTTP server metadata.
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