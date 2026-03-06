# BlackMap 4.0 🚀

[![Rust](https://img.shields.io/badge/Language-Rust-orange)](https://www.rust-lang.org/)
[![C](https://img.shields.io/badge/Language-C-blue)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Version](https://img.shields.io/badge/Version-4.0.0-success)](#)

**Fast, Stealthy, Reliable, and Scalable Network Reconnaissance Framework.**

BlackMap is a professional-grade reconnaissance framework built to conceptually compete with Nmap, Masscan, and RustScan while maintaining an entirely asynchronous architecture. BlackMap reaches 50,000+ concurrent sockets with zero scheduler-overhead via the Tokio runtime, while retaining deep low-level C FFI engines for stealth and raw packet crafting.

## 🌟 Key Features

* **Blazing Fast**: Written in pure Rust Async (Tokio). Capable of parallel resolving and scanning of millions of targets in seconds. 
* **Ultra Stealth**: Granular dynamic stealth profiles ranging from Level 0 (Aggressive) to Level 5 (Ghostly), modulating TTL manipulation, TCP timing, and jitter.
* **Service Fingerprinting DB**: A massive signature-loading system utilizing JSON definitions (regex matching) capable of extracting OS strings, product versions, and protocols.
* **Distributed Mode**: Native Master/Worker distributed cluster logic. Deploy workers across subnets!
* **Extensible Plugins**: Load powerful `.so` natively into the execution graph without recompiling.
* **Reliable Resolver**: Caches intelligently without ever freezing on failed hostnames via `trust-dns`.

## 📸 Screenshots

*(To be captured post-compilation)*

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

### Basic TCP Scan
```bash
# Scan a specific target prioritizing common ports
blackmap -p 22,80,443,3306 scanme.nmap.org
```

### Stealth and Versioning Scan
```bash
# Aggressive Scan (0), OS Fingerprinting, Service Detection, output as JSON
blackmap scanme.nmap.org -p- --stealth 0 -O -V -oJ scanme.json
```

### Distributed Scan Architecture (Clustering)
```bash
# Node 1: Start Master API server
blackmap --master 0.0.0.0:8000
# Node 2: Hook as worker 
blackmap --worker 192.168.1.50:8000
```

## 📚 Documentation
Please refer to the following resources:
- [ARCHITECTURE.md](ARCHITECTURE.md) - System overview and Rust/C interplay.
- [DEVELOPMENT.md](DEVELOPMENT.md) - How to setup the developer environment.
- [ROADMAP.md](ROADMAP.md) - Our goals through v4.5+.
- [CONTRIBUTING.md](CONTRIBUTING.md) - How to contribute PRs.

## 🤝 Roadmap & Open Source Community
This project was redesigned entirely around the Open Source community ethos. We encourage developers to experiment with writing rust-based plugins and expanding the JSON fingerprint DB. Please check our [Roadmap](ROADMAP.md) logic!