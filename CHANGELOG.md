# Changelog

All notable changes to this project will be documented in this file.

## [1.2] - 2024-03-12

### Added
- Service version detection with 30+ services
- OS fingerprinting via TCP analysis
- Multi-threaded scanning (up to 500 threads)
- Timing templates (T1-T5)
- Multiple output formats (XML, JSON, Grepable)
- Nmap-compatible output
- Advanced scan types (SYN, FIN, XMAS, NULL, ACK, UDP)
- Stealth features (decoy, scan delay, source spoofing)

### Changed
- Improved performance with non-blocking I/O
- Fixed IP resolution issues
- Enhanced error handling

### Removed
- Required -t flag (now positional argument)

## [1.1] - Previous Version

- Basic port scanning
- Banner grabbing
- Basic service detection
