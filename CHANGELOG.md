# BlackMap Changelog

## [2.0.0] - 2024-01-12

### ✨ Major Features

#### Rust FFI Integration
- Advanced service fingerprinting in Rust with confidence scoring
- Expanded protocol detection: HTTP, SSH, FTP, SMTP, MySQL, PostgreSQL, MongoDB, Redis, DNS, Telnet
- Detailed version extraction with server product identification
- Memory-safe FFI boundary with zero-copy string marshaling

#### Performance Optimization
- **9x faster output generation** (0.6s vs 6+ seconds for typical scans)
- Deferred result printing: scan results stored in memory, formatting happens post-scan
- Eliminated redundant port re-scanning during output generation
- Efficient per-port and per-host memory allocation

#### Simplified CLI
- **No root required by default** - TCP CONNECT scan as primary method
- Intuitive help message (20 lines vs 80+ in v1.0)
- Clear examples for common tasks
- Sensible option defaults

### 🎨 User Experience
- Clean, nmap-compatible output formatting with proper columnar alignment
- Visual latency reporting (rtt avg in milliseconds)
- Structured per-host summaries with port counts
- Progressive feedback during scanning

### 🛡 Code Quality
- GPL v3.0 licensing
- Hybrid C/Rust architecture for optimal performance and safety
- Comprehensive documentation (README, HACKING, COMPARISON, RUST_INTEGRATION)
- Proper memory lifecycle management across FFI boundary

### 📦 Build & Deployment
- Automatic Rust toolchain integration in Makefile
- Static linking of Rust library (libblackmap_rust.a)
- Cross-platform compatibility (Linux 4.18+, gcc 7.0+, Rust 1.56+)
- Single binary output with no runtime dependencies

### 🚀 New Protocols & Services Supported
- **HTTP**: Apache, nginx, IIS detection with version extraction
- **SSH**: OpenSSH and libssh identification
- **FTP**: vsftpd and ProFTPD automatic detection
- **SMTP**: Postfix and Sendmail identification
- **Databases**: MySQL, PostgreSQL, MongoDB, Redis with version strings
- **Others**: DNS and Telnet with confidence scoring

### 🔧 Rust Module Enhancements
- Regex-based pattern matching for all protocols
- Confidence scoring (0-100) per detection
- Extra fields metadata (product name, implementation, server details)
- JSON serialization with optional fields for clean output
- Lazy-static regex compilation for performance

### 📊 Output Formats
- Normal (pretty-printed with alignment)
- JSON (fully structured for automation)
- XML (Metasploit/Nessus compatible)
- Grepable (log-friendly format)
- SQLite (queryable database)
- HTML (interactive visualization)
- Markdown (documentation output)

## [1.0.0] - 2024-01-05

### Initial Release Features
- Multi-engine architecture (select, epoll, io_uring, AF_XDP)
- TCP scanning (SYN, CONNECT, FIN, NULL, XMAS)
- Basic service detection
- Proxy compatibility (proxychains, torsocks auto-detection)
- Multiple output formats
- Port range parsing
- Timing templates (T0-T5)

---

## Installation

### From Source (v2.0.0)
```bash
git clone https://github.com/Brian-Rojo/Blackmap.git
cd Blackmap
make clean
make
./blackmap --version  # BlackMap v2.0.0
```

### Key Improvements from v1.0 to v2.0

| Aspect | v1.0 | v2.0 | Improvement |
|--------|------|------|------------|
| **Performance** | 6+ seconds (3 ports) | 0.6 seconds | 10x faster |
| **Root Requirement** | Yes (default SYN) | No (default CONNECT) | More flexible |
| **Service Detection** | 4 protocols | 10+ protocols | 2.5x coverage |
| **CLI Complexity** | 80+ help lines | 20 help lines | 4x simpler |
| **Language** | C only | C + Rust | Better safety |
| **License** | Undecided | GPL v3 | Free & open |
| **Version Detection** | Basic patterns | Rust + confidence | More reliable |

## Security & Compatibility

- ✓ No unsafe Rust code exposed to C layer
- ✓ Proper memory cleanup on error paths
- ✓ Signal handling for clean shutdown
- ✓ Valid on Linux 4.18+ (most systems)
- ✓ Tested on Debian, Fedora, Ubuntu
- ✓ Cross-architecture (x86_64, ARM64)

## Contributors

- **Brian-Rojo** - Project creator and maintainer

## Future Roadmap

### v2.1 (Q1 2024)
- Machine learning-based service classification
- Vulnerability detection from banner analysis
- Extended protocol support (SMB, RDP, HTTP/2)
- Performance benchmarking suite

### v2.2 (Q2 2024)
- Interactive web UI for result visualization
- Distributed scanning coordination
- Custom signature DSL
- Plugin system for user-defined detectors

### v3.0 (Q3 2024)
- Full IPv6 support
- Kernel eBPF integration for kernel-level filtering
- Real-time packet capture and analysis
- Advanced evasion techniques

## License

BlackMap is licensed under the GNU General Public License v3.0 (GPL-3.0).
See [LICENSE](LICENSE) for the full text.

Free software for security professionals.
