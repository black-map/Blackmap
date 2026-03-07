# BlackMap - Development Guide

## Project Structure

```
blackmap/
├── src/                 # Source code
│   ├── core/           # Main orchestration
│   ├── engines/        # I/O engines (select, epoll, io_uring, AF_XDP)
│   ├── netstack/       # Custom TCP/IP stack
│   ├── scanning/       # Scan type implementations
│   ├── fingerprinting/ # OS & service detection
│   ├── evasion/        # IDS/IPS evasion techniques
│   ├── scripting/      # LuaJIT/NSE integration
│   ├── output/         # Output format handlers
│   ├── utils/          # Utility functions
│   └── compat/         # Proxy & fallback support
├── include/            # Public headers
├── scripts/            # NSE/BlackScript scripts
├── data/              # Fingerprints & probes
├── tests/             # Unit & integration tests
├── docs/              # Documentation
├── Makefile           # Build system
└── README.md
```

## Building

### Basic Build

```bash
cd /path/to/blackmap
make clean
make
```

### Debug Build

```bash
make debug
```

### With Optional Features

```bash
# Install liburing first (optional)
sudo apt-get install liburing-dev

# Rebuild
make clean
make
```

## Key Components

### 1. Core Module (`src/core/`)
- **main.c**: CLI parsing with getopt
- **blackmap.c**: Main orchestration logic
- **config.c**: Configuration management
- **signals.c**: Signal handling

### 2. I/O Engines (`src/engines/`)
Pluggable I/O multiplexing:
- **io_uring_engine.c**: Linux io_uring (10M+ pps target)
- **xdp_engine.c**: AF_XDP zero-copy mode
- **epoll_engine.c**: Standard epoll
- **select_engine.c**: Fallback select()

Single interface:
```c
typedef struct {
    const char *name;
    int (*init)(void);
    void (*cleanup)(void);
    int (*submit_packet)(const uint8_t *packet, uint32_t len);
    int (*get_responses)(uint8_t *buffer, uint32_t max_len, uint32_t *packets);
    int (*poll_timeout)(uint32_t timeout_ms);
    bool (*is_supported)(void);
} io_engine_t;
```

### 3. Network Stack (`src/netstack/`)
Custom TCP/IP implementation:
- Packet construction (IP, TCP, UDP, ICMP, SCTP)
- Checksum calculation (hardware-optimized fallback)
- Raw socket operations
- Custom TCP state machine

### 4. Scanning (`src/scanning/`)
All Nmap-compatible scans:
- TCP: CONNECT, SYN, FIN, NULL, XMAS, ACK, WINDOW, MAIMON, IDLE
- UDP: Adaptive retransmission
- SCTP: INIT, COOKIE-ECHO
- IP Protocol: Detection of various protocols
- Ping: Host discovery

### 5. Fingerprinting (`src/fingerprinting/`)
- OS Detection: 5000+ fingerprints (vs Nmap's 2600)
- Service Detection: 10000+ probes
- Version Detection: Banner grabbing + analysis
- Virtualization & Container Detection

### 6. Evasion (`src/evasion/`)
- IP Fragmentation: Custom MTU
- Decoy Generation: Realistic timing
- Timing Obfuscation: T0-T5 templates
- Payload Obfuscation: Random/custom data
- Personality Spoofing: OS-specific behavior

### 7. Output (`src/output/`)
Multiple format handlers:
- Normal (human-readable)
- XML (Metasploit/Nessus compatible)
- Grepable (one-line per host)
- JSON (modern structured)
- SQLite (queryable database)
- HTML (interactive visualization)
- Markdown (documentation)

### 8. Scripting (`src/scripting/`)
- LuaJIT integration (10x Lua 5.4 speed)
- Nmap Scripting Engine (NSE) compatibility
- Async/await for I/O operations
- Built-in protocol clients

### 9. Utilities (`src/utils/`)
- Random number generation (CSPRNG)
- Logging & statistics
- Memory pooling
- Bitmap operations for port tracking

### 10. Compatibility (`src/compat/`)
- proxychains4 detection
- torsocks detection
- Kernel feature detection
- Graceful fallback chain

## Development Workflow

### Adding a New Scan Type

1. **Create scanner module** (`src/scanning/scanner_newtype.c`):
```c
int scanner_newtype_init(void) { ... }
int scanner_newtype_scan_host(host_info_t *host) { ... }
void scanner_newtype_cleanup(void) { ... }
```

2. **Register in config** (`include/blackmap.h`):
```c
typedef enum {
    // ... existing types
    SCAN_TYPE_NEWTYPE = 15
} scan_type_t;
```

3. **Parse CLI option** (`src/core/main.c`):
```c
case 'n':
    g_config->scan_type = SCAN_TYPE_NEWTYPE;
    break;
```

4. **Build & test**:
```bash
make clean
make
./blackmap -sNewType 127.0.0.1
```

### Adding a New Output Format

1. **Create formatter** (`src/output/output_newformat.c`):
```c
int output_newformat(FILE *fp, host_info_t *host) {
    // Serialize host to format
    return 0;
}
```

2. **Register handler** (`src/output/output.c`):
```c
if (g_config->output_newformat) {
    FILE *fp = config->output_file[0] ? 
               fopen(config->output_file, "a") : stdout;
    output_newformat(fp, host);
    if (fp != stdout) fclose(fp);
}
```

3. **Build & test**:
```bash
make clean
make
./blackmap -oNewFormat results 127.0.0.1
```

## Testing

### Unit Tests

```bash
# Create test in tests/
gcc -I./include -o test_module tests/test_module.c src/module/*.c

./test_module
```

### Integration Tests

```bash
# Scan localhost (safe)
./blackmap -sS -p 22,80,443 127.0.0.1

# Scan with timing template
./blackmap -T3 -p 1-1024 192.168.1.0/24

# Version detection + OS fingerprinting
./blackmap -sV -O 127.0.0.1

# Output to multiple formats
./blackmap -oA results 127.0.0.1
```

## Performance Optimization

### io_uring Configuration

Tune for maximum throughput:
```c
#define QUEUE_DEPTH 8192     // Increase from 4096
#define RING_SIZE 65536      // Increase batch size
// params.flags |= IORING_SETUP_IOPOLL; // Poll mode (no interrupts)
// params.flags |= IORING_SETUP_SQPOLL; // Kernel SQ polling
```

### Memory Usage

- Use memory pools for packet buffers
- Bitmap for port tracking (1 Mbport map = ~8KB)
- String interning for service names

### CPU Cache

- Align frequently accessed structures (64-byte cache line)
- Batch packet operations
- Minimize context switches

## Contributing

### Code Style

- K&R style indentation (4 spaces)
- Prefix statics with `module_`
- Comment non-obvious logic
- Handle errors explicitly

### Security

- Validate all user input
- Use safe string functions
- Bounds check arrays
- Run AddressSanitizer in CI

### Testing Before PR

```bash
make clean
make debug
valgrind --leak-check=full ./blackmap -p 22,80 127.0.0.1
```

## Profiling

### CPU Profiling

```bash
gcc -pg -O2 -I./include -o blackmap ... 
./blackmap [options]
gprof blackmap gmon.out
```

### Memory Profiling

```bash
make debug
valgrind --tool=massif ./blackmap -p 1-10000 127.0.0.1
ms_print massif.out.<num>
```

## Benchmarking

Target metrics vs Nmap 7.94:

| Metric | Target | Measurement |
|--------|--------|-------------|
| SYN scan (localhost, 1K ports) | 10M pps | Use `time` |
| Memory (1M hosts) | <500MB | `top` |
| Startup overhead | <10ms | `time` |
| OS detection accuracy | >95% | Custom test suite |

## Next Steps (v1.0 Phase)

- [ ] Complete TCP/UDP scanning implementation
- [ ] Implement full fingerprinting engine
- [ ] Integrate LuaJIT scripting
- [ ] Add all output formats
- [ ] Performance benchmarking & optimization
- [ ] Comprehensive test suite (80%+ coverage)
- [ ] Man pages & full documentation
- [ ] Package for Linux distributions

---

**Happy hacking! For questions or contributions, open an issue or PR.**
