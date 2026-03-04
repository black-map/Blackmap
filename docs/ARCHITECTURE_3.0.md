# BlackMap 3.0 - Architecture & Design Document

**Version**: 3.0.0-alpha  
**Date**: March 4, 2026  
**Author**: Architecture Team

---

## 1. Executive Summary

BlackMap 3.0 is a complete architectural rewrite of the network scanning engine, designed around modern Linux epoll-based I/O, modular component design, and advanced behavior profiling. The architecture separates concerns into discrete modules that communicate through clean interfaces, enabling future extensibility without core modifications.

**Key Design Principles:**
- **Modularity**: Each component has single responsibility
- **Performance**: Non-blocking I/O, buffer pooling, zero-copy FFI
- **Configurability**: 5 stealth levels with fine-grained control
- **Measurability**: Comprehensive metrics at every layer
- **Safety**: C with -Wall -Wextra -Werror, Rust FFI with no unsafe in public API

---

## 2. Overall Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        CLI Interface                            │
│                  (main.c - thin entry point)                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│                   ORCHESTRATION LAYER (blackmap.c)              │
│                                                                 │
│  target_ips[]  ───→  [Scheduler]  ───→  [Network Engine]       │
│        ↓                                        ↓               │
│    ports[]  ────→  [Stealth Control] ────→ [Connections]       │
│                                                ↓               │
│                                    [Rust Analysis Engine]       │
│                                    (Service Fingerprinting)     │
│                                                ↓               │
│                                    [Metrics Collection]         │
│                                                ↓               │
│                                    [Output Formatting]          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

**Data Flow**: 
```
Targets & Ports 
    ↓
Scan Plan Creation
    ↓
Scheduler Enqueue
    ↓
Network Engine (epoll loop)
    ├─ Port Ordering (stealth)
    ├─ Timing Control (stealth)
    ├─ Connection State Machine
    ├─ Banner Grabbing
    └─ RTT Measurement
    ↓
Rust Analysis (FFI)
    ├─ Pattern Matching
    ├─ Version Extraction
    ├─ Confidence Scoring
    └─ JSON Serialization
    ↓
Metrics Recording
    ├─ RTT Statistics
    ├─ Throughput Calculation
    ├─ Success Rates
    └─ Concurrency Tracking
    ↓
Results Storage
    ↓
Output Formatting (JSON/Normal/CSV/etc)
```

---

## 3. Core Modules

### 3.1 Network Engine (C)

**File**: `src/core/network/network.c`  
**Header**: `include/blackmap3/network.h`  
**Responsibility**: Non-blocking socket I/O, connection lifecycle management

**Key Components:**
- **epoll Integration**: Linux-specific efficient event multiplexing
- **Connection State Machine**: 9 states (INIT → CONNECTING → OPEN → READING → CLOSED/TIMEOUT/ERROR)
- **Buffer Pool**: Pre-allocated buffers to avoid allocation during scanning
- **RTT Measurement**: Precise timing using `CLOCK_MONOTONIC`

**Connection Lifecycle:**
```
socket() [non-blocking]
    ↓
connect() [EINPROGRESS]
    ↓
epoll_wait() [EPOLLOUT]
    ↓
TCP handshake complete
    ↓
recv() banner data [EPOLLIN]
    ↓
Connection close
    ↓
Metrics recorded
```

**Public API:**
```c
network_engine_t* network_engine_init(...)
int network_queue_connection(...)  // Async enqueue
int network_process_batch(...)     // Blocking until timeout_logic_ms
connection_get_state(...)          // Query result
connection_get_banner(...)         // Extract banner data
network_get_metrics(...)           // Performance stats
```

**Performance Characteristics:**
- Can manage 10,000+ concurrent connections per epoll instance
- No thread creation needed (single-threaded event loop)
- Memory: ~4KB per connection slot + buffer pool

---

### 3.2 Scheduler (C)

**File**: `src/core/scheduler/scheduler.c`  
**Header**: `include/blackmap3/scheduler.h`  
**Responsibility**: Task queue management, port ordering, concurrency enforcement

**Key Features:**
- **Circular Task Queue**: Efficient memory usage, FIFO ordering
- **Per-Host Concurrency Tracking**: Prevents single-host DOS-like behavior
- **Global Concurrency Enforcement**: Total connection limit per scan
- **Port Ordering Strategies**: Random, ascending, descending, common-first

**Concurrency Model:**
```
Global Limit: max_concurrency_global = 128
Per-Host Limit: max_concurrency_per_host = 16

While there are tasks:
  task = scheduler_next_task()
  if (active_connections < global_limit &&
      per_host_connections[task->host] < per_host_limit):
    network_queue_connection(task)
```

**Public API:**
```c
scheduler_enqueue_plan(...)     // Load all tasks
task_t* scheduler_next_task()   // Get next eligible task
scheduler_mark_complete(...)    // Notify task finished
scheduler_is_finished(...)      // Check if done
scheduler_can_connect_to_host(...)  // Check limits
```

---

### 3.3 Stealth System (C)

**File**: `src/core/stealth/stealth.c`  
**Header**: `include/blackmap3/stealth.h`  
**Responsibility**: Behavior profiles, timing control, adaptive pacing

**5 Stealth Levels:**

| Level | Name | Global CC | Per-Host CC | Jitter | Delays | Burst | Backoff |
|-------|------|-----------|-------------|--------|--------|-------|---------|
| 0 | Performance | 256 | 64 | 0% | 0ms | 256 | No |
| 1 | Balanced | 128 | 32 | 10% | 1ms | 128 | Yes |
| 2 | Low Noise | 32 | 8 | 25% | 10ms | 32 | Yes |
| 3 | Conservative | 8 | 2 | 50% | 100ms | 8 | Yes |
| 4 | Ultra Conservative | 1 | 1 | 80% | 500ms | 1 | Yes |

**Behavior Examples:**

*Level 0 (Performance)*:
```
for port in ports:
  connect(host, port)  // No delays, full speed
```

*Level 2 (Low Noise)*:
```
randomize_port_order()
for port in ports:
  sleep(10ms + random(0-3ms))
  connect(host, port)
  if burst_counter >= 32:
    sleep(500ms)
    burst_counter = 0
```

*Level 4 (Ultra Conservative)*:
```
randomize_port_order()
randomize_host_order()
for host in hosts:
  for port in randomized_ports:
    sleep(500ms + random(0-400ms))
    connect(host, port)
    wait_for_complete()
  sleep(2000ms)  // Inter-host pause
```

**Public API:**
```c
stealth_get_preset(level)           // Get preconfigured levels
stealth_get_pre_connect_delay_us()  // Delay before connect
stealth_get_post_connect_pause_us() // Pause after connect
stealth_get_adaptive_timeout_ms()   // RTT-aware timeout
stealth_should_detect_version()     // Skip banners?
stealth_get_backoff_delay_ms()      // Exponential backoff on timeout
```

---

### 3.4 Analysis Engine (Rust → C FFI)

**Files**: 
- `rust/src/lib.rs` (Rust implementation)
- `include/blackmap3/analysis.h` (C header)

**Responsibility**: Service fingerprinting, version extraction, confidence scoring

**Architecture:**
```
Banner bytes (C)
    ↓
[FFI Boundary - CStr marshaling]
    ↓
Rust Pattern Matching
    ├─ HTTP (Apache/nginx/IIS detection)
    ├─ SSH (OpenSSH identification)
    ├─ FTP (vsftpd/ProFTPD)
    ├─ SMTP (Postfix/Sendmail)
    ├─ Databases (MySQL/PostgreSQL/MongoDB/Redis)
    └─ Others (DNS, Telnet)
    ↓
JSON Serialization (serde)
    ↓
[FFI Boundary - CString allocation]
    ↓
Result in C
```

**Service Info Structure:**
```c
typedef struct {
    char *service;           // "HTTP", "SSH", "FTP"
    char *implementation;    // "Apache", "OpenSSH", "nginx"
    char *version;           // "2.4.41", "7.4p1"
    uint8_t confidence;      // 0-100 score
    char **extra_fields;     // Metadata key-value pairs
} service_info_t;
```

**Zero-Copy Optimization:**
- Input: Rust receives raw pointer + length (no allocation)
- Output: Rust allocates `CString`, C receives pointer
- Cleanup: C calls `analysis_free_string()` when done

**Public API:**
```c
service_info_t* analysis_parse_banner(const uint8_t *banner, size_t len)
void analysis_free_service_info(service_info_t *info)
const char* analysis_get_service_from_port(uint16_t port)
int analysis_is_likely_valid_banner(const uint8_t *b, size_t len)
char* analysis_service_to_json(const service_info_t *info)
analysis_metrics_t analysis_get_metrics(void)
```

---

### 3.5 Metrics Engine (C)

**File**: `src/metrics/metrics.c`  
**Header**: `include/blackmap3/metrics.h`  
**Responsibility**: Real-time performance tracking and reporting

**Measured Events:**
- Per-connection: RTT, state transitions, errors
- Per-scan: Total connections, timeouts, success rate
- Per-protocol: Services detected, version extraction success
- Aggregate: Throughput, peak concurrency, average latency

**Metrics Snapshot:**
```c
typedef struct {
    uint64_t total_connections_attempted;
    uint64_t total_connections_open;
    uint64_t total_timeouts;
    uint64_t total_errors;
    uint64_t total_rtt_us;          // Sum for averaging
    uint32_t rtt_measurements;      // Count
    uint64_t min_rtt_us, max_rtt_us;// Range
    uint64_t bytes_sent, bytes_received;
    uint32_t services_detected;
    uint32_t peak_concurrency;
} metrics_snapshot_t;
```

**Output Formats:**
```
CLI Table:
┌──────────────────────────────────┐
│ BLACKMAP SCAN METRICS             │
├──────────────────────────────────┤
│ Elapsed:      2.34 seconds        │
│ Connections:  150/1000            │
│ Success Rate: 95.2%               │
│ Avg RTT:      12.4 ms             │
└──────────────────────────────────┘

JSON:
{
  "elapsed_seconds": 2.34,
  "total_connections_attempted": 1000,
  "total_connections_open": 152,
  "success_rate_percent": 95.2,
  "avg_rtt_ms": 12.4
}
```

---

## 4. Data Structures

### 4.1 Connection State

```c
typedef struct {
    int fd;                       // Socket FD
    conn_state_t state;           // Current state
    struct sockaddr_in addr;      // Target address
    uint16_t port;                // Target port
    
    struct timespec start_time;   // Connection start
    struct timespec connect_time; // TCP completion
    uint64_t rtt_us;              // Round-trip time
    
    uint8_t *read_buffer;         // Banner data
    size_t read_bytes;            // Bytes received
    
    uint32_t timeout_ms;          // Per-connection timeout
    uint32_t elapsed_ms;          // Elapsed time
    
    void *host_context;           // Reference to parent host
} connection_t;
```

### 4.2 Scheduler Task

```c
typedef struct {
    uint32_t host_index;          // Which host in plan
    uint16_t port;                // Port number
    probe_type_t probe_type;      // Probe type (TCP, UDP, etc)
} task_t;
```

### 4.3 Stealth Configuration

```c
typedef struct {
    stealth_level_t level;
    
    uint32_t max_concurrency_global;
    uint32_t max_concurrency_per_host;
    
    uint32_t jitter_percent;             // Random timing variance
    int enable_port_randomization;       // Shuffle port order
    
    uint32_t delay_between_ports_ms;     // Forced delay
    uint32_t burst_size;                 // Packets before pause
    uint32_t pause_after_burst_ms;       // Required pause
    
    int enable_rtt_awareness;            // Adapt timeouts?
    int skip_version_detection;          // Skip banners?
} stealth_config_t;
```

---

## 5. Integration Points

### 5.1 C ↔ Rust FFI

**Banner Processing Flow:**

```c
// In C (network.c):
connection_t *conn = ...;
const uint8_t *banner = conn->read_buffer;
size_t banner_size = conn->read_bytes;

// Call Rust analysis
service_info_t *result = analysis_parse_banner(banner, banner_size);

// Process result
if (result && result->confidence > 50) {
    printf("Service: %s v%s\n", result->service, result->version);
}

// Free Rust allocation
analysis_free_service_info(result);
```

**FFI Safety Guarantees:**
1. No unsafe code in public Rust API
2. C handles all pointer validation
3. Rust uses CString for string marshaling
4. All allocations tracked (C owns memory lifecycle)

---

## 6. Compilation & Safety

### 6.1 C Compilation Flags

```makefile
CFLAGS = -std=c99 -Wall -Wextra -Werror \
         -O2 -fPIC \
         -fsanitize=address \
         -fsanitize=undefined \
         -fno-sanitize-recover=undefined

LDFLAGS = -fsanitize=address -fsanitize=undefined
```

### 6.2 Rust Compilation

```toml
[profile.release]
opt-level = 3
lto = true
codegen-units = 1

[lib]
crate-type = ["staticlib"]
```

```bash
cargo build --release
cargo clippy -- -D warnings
cargo test --release
```

---

## 7. Performance Expectations

### Throughput
- **Performance profile (Level 0)**: 1000-5000 ports/second (localhost)
- **Balanced profile (Level 1)**: 200-500 ports/second
- **Conservative (Level 3)**: 10-50 ports/second
- Note: Real throughput depends on target network, RTT, and response time

### Memory
- **Per connection**: ~4KB base + buffer pool slots
- **Network engine**: ~500KB (epoll + connection tracking)
- **Scheduler**: ~16KB (4K task queue)
- **Stealth context**: ~1KB
- **Total for 10,000 target ports**: <5MB

### Concurrency
- **epoll max FDs**: Limited by system (usually 1024-65536)
- **Effective concurrency**: Controlled by scheduler limits (default 128 global)
- **True parallelism**: Single-threaded, efficient multiplexing

---

## 8. Testing Strategy

### Unit Tests
- Scheduler: Queue enqueue/dequeue, concurrency limits
- Network: Connection state transitions, RTT measurement
- Stealth: Preset configurations, adaptive delays
- Metrics: Aggregation, calculation correctness

### Integration Tests
- Full scan: Target → Scheduler → Network → Analysis → Metrics
- Stealth levels: Verify timing profiles work as configured
- FFI boundary: C↔Rust string marshaling, memory cleanup
- Error handling: Lost connections, timeouts, invalid banners

### Benchmarks
- Throughput: Ports/second at each stealth level
- Latency: Total time from start to finish
- Memory: Peak usage during large scans
- Regression: Prevent performance degradation

---

## 9. Future Extensions

### Without Modifying Core
- **New Protocols**: Add to Rust pattern database
- **Output Formats**: Extend output formatting layer
- **Custom Stealth Profiles**: New preconfigured levels
- **Vulnerability Detection**: Parse banners for known CVEs

### With Modular Redesign
- **Multi-threading**: Thread pool for parallel scanning
- **IPv6 Support**: New address structure handling
- **UDP/SCTP**: New protocol handlers in network engine
- **Raw Sockets**: Bypass TCP CONNECT for SYN scans

---

## 10. References

- **Linux epoll(7)**: `man 7 epoll`
- **Socket Options**: `man 7 socket`
- **TCP Socket Lifecycle**: Stevens "Unix Network Programming"
- **Rust FFI**: https://docs.rust-embedded.org/book/c-interop/index.html

---

**Document Version**: 1.0  
**Last Updated**: March 4, 2026  
**Status**: Active Development
