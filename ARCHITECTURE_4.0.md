# BlackMap 4.0 Architecture

## Overview

BlackMap 4.0 is a professional-grade network reconnaissance framework written primarily in **Rust** with targeted C components for low-level network operations.

**Language Distribution:**
- Rust: 85%
- C: 15%

## Core Design Principles

1. **Async-First**: All I/O is non-blocking and event-driven
2. **Safety**: Minimize unsafe code, leverage Rust's type system
3. **Modularity**: Plugin-based architecture for extensibility
4. **Performance**: Support 50,000+ concurrent connections
5. **Stealth**: Multi-level evasion capabilities
6. **Reliability**: Robust error handling and recovery

## Architecture Layers

```
┌─────────────────────────────────────────────────────────┐
│                   CLI Interface (Rust)                  │
├─────────────────────────────────────────────────────────┤
│            Configuration & Plugin System                │
├─────────────────────────────────────────────────────────┤
│              Scanning Orchestrator (Rust)               │
│  ├─ Scheduler  │ Rate Limiter │ Timing Engine          │
├─────────────────────────────────────────────────────────┤
│            Async Runtime & Networking (Rust)            │
│  ├─ tokio  │ DNS Resolver │ Connection Pool            │
├─────────────────────────────────────────────────────────┤
│         Service Detection Pipeline (Rust)               │
│  ├─ Banner Grabbing │ Fingerprinting │ OS Detection    │
├─────────────────────────────────────────────────────────┤
│            Stealth Engine (Rust + C)                    │
│  ├─ Fragmentation │ TTL Manipulation │ Jitter         │
├─────────────────────────────────────────────────────────┤
│          Low-Level Network (C)                          │
│  ├─ Raw Sockets │ SYN Scanning │ Packet Crafting      │
└─────────────────────────────────────────────────────────┘
```

## Module Breakdown

### 1. CLI Interface (Rust)
- Command-line parsing using `clap`
- Profile management
- Config file handling
- Output formatting

### 2. DNS Resolver (Rust)
**Critical Fix for BlackMap 3.x bugs**

```rust
pub struct DnsResolver {
    cache: Arc<Mutex<DnsCache>>,
    client: TrustDnsResolver,
    timeout: Duration,
    parallel_limit: usize,
}

impl DnsResolver {
    pub async fn resolve_batch(&self, targets: Vec<String>) -> Result<Vec<ResolvedHost>>;
    pub async fn resolve_ipv4(&self, host: &str) -> Result<Vec<IpAddr>>;
    pub async fn resolve_ipv6(&self, host: &str) -> Result<Vec<IpAddr>>;
    pub async fn reverse_lookup(&self, ip: IpAddr) -> Result<Vec<String>>;
}
```

**Features:**
- IPv4/IPv6 support
- Parallel resolution
- DNS caching
- Configurable resolvers
- Timeout control
- Fallback servers
- CIDR expansion
- Domain wildcard support

### 3. Scheduling Engine (Rust)
- Task queue with priority
- Rate limiting (packets/sec)
- Per-host concurrency limits
- Adaptive timing based on RTT
- Jitter injection

### 4. Scanning Engine (Rust + C)

#### TCP Scans (C for SYN, Rust for CONNECT)
```c
// SYN scan (C)
int tcp_syn_scan(int sock, struct sockaddr_in *addr);

// FIN, NULL, XMAS scans (C)
int tcp_fin_scan(int sock, struct sockaddr_in *addr);
int tcp_null_scan(int sock, struct sockaddr_in *addr);
int tcp_xmas_scan(int sock, struct sockaddr_in *addr);
```

#### TCP CONNECT (Rust)
```rust
pub async fn tcp_connect_scan(
    addr: SocketAddr,
    timeout: Duration,
) -> Result<PortState>;
```

#### UDP Scanning (Rust)
```rust
pub async fn udp_scan(
    addr: SocketAddr,
    probe: &UdpProbe,
    timeout: Duration,
) -> Result<PortState>;
```

#### Host Discovery (Rust)
- ICMP Echo
- TCP ACK
- TCP SYN
- TCP CONNECT

### 5. Service Detection (Rust)

```rust
pub struct ServiceDetector {
    fingerprints: Vec<ServiceFingerprint>,
    cache: Arc<Mutex<ServiceCache>>,
    probe_queue: Vec<ServiceProbe>,
}

pub struct ServiceFingerprint {
    service_name: String,
    port: u16,
    protocol: Protocol,
    patterns: Vec<BannerPattern>,
    version_regex: Option<Regex>,
    confidence: f32,
}
```

**Capabilities:**
- Banner grabbing
- Protocol analysis
- Version extraction
- Confidence scoring
- 500+ service signatures

### 6. OS Fingerprinting (Rust)

```rust
pub struct OSDetector {
    ttl_signatures: Vec<TtlSignature>,
    window_signatures: Vec<WindowSignature>,
    behavior_profiles: Vec<BehaviorProfile>,
}

pub enum OSType {
    Linux,
    Windows,
    BSD,
    MacOS,
    NetworkDevice,
    EmbeddedSystem,
    Unknown,
}
```

### 7. Stealth Engine (Rust + C)

```rust
pub struct StealthConfig {
    level: StealthLevel,  // 0-5: Performance to Paranoid
    packet_fragmentation: bool,
    tcp_option_randomize: bool,
    ttl_manipulation: bool,
    packet_padding: bool,
    decoy_scan: bool,
    jitter_percent: u32,
    rate_limit_pps: u32,
    randomize_port_order: bool,
}

pub enum StealthLevel {
    Performance = 0,
    Normal = 1,
    Quiet = 2,
    Stealth = 3,
    UltraStealth = 4,
    Paranoid = 5,
}
```

**Stealth Techniques:**
- Packet fragmentation (C)
- TCP option randomization (C)
- TTL manipulation (C)
- Packet padding (C)
- Decoy scanning (Rust)
- Jitter injection (Rust)
- Adaptive timing (Rust)
- Port order randomization (Rust)

### 8. Plugin System (Rust)

```rust
pub trait Plugin: Send + Sync {
    fn name(&self) -> &str;
    fn version(&self) -> &str;
    fn on_port_open(&self, port: u16, service: &str);
    fn on_service_detected(&self, service: &ServiceInfo);
    fn on_host_up(&self, host: &HostInfo);
}

pub struct PluginManager {
    plugins: Vec<Box<dyn Plugin>>,
}
```

### 9. Distributed Scanning (Rust)

```rust
pub struct MasterNode {
    workers: Vec<WorkerConnection>,
    task_queue: Arc<Mutex<TaskQueue>>,
}

pub struct WorkerNode {
    scanner: ScannerEngine,
    master_addr: SocketAddr,
}
```

## Data Flow

```
Input (targets, ports, options)
    ↓
[DNS Resolution] ← Critical fix point
    ↓
[Target Expansion & Validation]
    ↓
[Task Generation & Scheduling]
    ↓
[Scanning Loop] (async, event-driven)
    ├─ Protocol-specific probes
    ├─ Connection state management
    ├─ Banner grabbing
    └─ Service detection
    ↓
[Results Aggregation]
    ↓
[Output Formatting] (JSON, XML, Table)
```

## Critical Bugs to Fix

1. **Hanging after host discovery**: Root cause - scheduler not properly dispatching tasks
   - Fix: Implement proper tokio task spawning with explicit completion tracking
   
2. **DNS resolver unreliable**: Root cause - improper error handling and timeouts
   - Fix: Use trust-dns with proper async/await and timeout management
   
3. **Port parsing returns 0 ports**: Root cause - edge case in port range parsing
   - Fix: Comprehensive test coverage and validation
   
4. **Select fallback unreliable**: Root cause - incomplete select() implementation
   - Fix: Deprecate in favor of epoll/io-uring async implementation
   
5. **Scheduler doesn't dispatch**: Root cause - improper task queue management
   - Fix: Use tokio channels and proper concurrency primitives

## Performance Targets

| Metric | Target |
|--------|--------|
| Concurrent Sockets | 50,000+ |
| Port Scan Rate | 100,000+ pps |
| DNS Resolution | 1000+ parallel |
| Memory/Connection | <1KB |
| Startup Time | <100ms |
| Shutdown Time | <100ms |

## Output Formats

```json
{
  "scan": {
    "start_time": "2026-03-05T10:00:00Z",
    "end_time": "2026-03-05T10:05:00Z",
    "command": "blackmap -sV -p- scanme.nmap.org",
    "hosts": [
      {
        "ip": "45.33.32.156",
        "hostname": "scanme.nmap.org",
        "status": "up",
        "uptime": "2 days",
        "ports": [
          {
            "port": 22,
            "state": "open",
            "protocol": "tcp",
            "service": "ssh",
            "version": "OpenSSH 7.4",
            "product": "OpenSSH",
            "os_cpe": "cpe:/o:linux:linux_kernel"
          }
        ]
      }
    ],
    "summary": {
      "total_hosts": 1,
      "hosts_up": 1,
      "total_ports": 1000,
      "open": 5,
      "closed": 995
    }
  }
}
```

## Dependencies

### Core
```toml
tokio = { version = "1", features = ["full"] }
trust-dns-resolver = "0.23"
trust-dns-proto = "0.23"
serde = { version = "1", features = ["derive"] }
serde_json = "1"
clap = { version = "4", features = ["derive"] }
regex = "1"
```

### Optional
```toml
hyper = "1"  # HTTP probing
quinn = "0.11"  # QUIC support
libpnet = "0.34"  # Raw sockets
pcap = "1"  # Packet capture
```

## Testing Strategy

1. **Unit Tests**: Core logic (DNS, scheduling, parsing)
2. **Integration Tests**: Full scan workflows
3. **Performance Tests**: Concurrency limits, throughput
4. **Security Tests**: Stealth validation

## Development Phases

### Phase 1: Foundation
- [ ] Rust project setup
- [ ] Fix DNS resolver
- [ ] Fix hanging scan bug
- [ ] Basic async scanning engine

### Phase 2: Core Features
- [ ] Service detection
- [ ] OS fingerprinting
- [ ] Multiple protocols
- [ ] Plugin system

### Phase 3: Advanced
- [ ] Stealth engine
- [ ] Distributed scanning
- [ ] Fingerprint database
- [ ] Output formatting

### Phase 4: Polish
- [ ] Documentation
- [ ] Performance tuning
- [ ] Testing
- [ ] Release preparation

