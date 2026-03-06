# BlackMap Installation Guide

BlackMap 4.0 is a hybrid Rust and C project designed for Unix-like systems.

## Prerequisites

Before starting, ensure your system has the required build dependencies:

### Ubuntu / Debian
```bash
sudo apt-get update
sudo apt-get install -y build-essential libpcap-dev pkg-config cmake
```

### Fedora / RHEL
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install libpcap-devel pkg-config cmake
```

### Arch Linux
```bash
sudo pacman -S base-devel libpcap cmake
```

## Installing Rust

BlackMap 4.0 is now primarily a Rust application (2021 Edition).

```bash
# Download and execute the rustup installer
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Source your current shell profile
source $HOME/.cargo/env
```

## Compiling BlackMap

```bash
# Clone the repository
git clone https://github.com/Brian-Rojo/Blackmap.git
cd Blackmap/rust

# Build the release profile
cargo build --release

# The compiled binary will be located in the target directory
./target/release/blackmap --version
```

## System-Wide Installation

To install BlackMap system-wide, simply copy the binary to `/usr/local/bin/`.

```bash
sudo cp target/release/blackmap /usr/local/bin/
```

To run Advanced packet crafting techniques (like `tcp-syn` or `icmp-echo`), you currently require `CAP_NET_RAW`.

```bash
# Allow blackmap to open raw sockets without running as full root 
sudo setcap cap_net_raw,cap_net_admin=eip /usr/local/bin/blackmap
```
