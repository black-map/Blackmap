# BlackMap v3.2 - Next-Generation Reconnaissance Framework

**Version**: 3.2.0  
**Date**: March 5, 2026  
**Status**: Architecture Design & Implementation Roadmap  
**Author**: Senior Systems Architecture Team

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [System Overview](#system-overview)
3. [Core Architecture Improvements](#core-architecture-improvements)
4. [Module Specifications](#module-specifications)
5. [Data Structures & API Design](#data-structures--api-design)
6. [FFI Boundaries & Type Safety](#ffi-boundaries--type-safety)
7. [Performance & Scalability](#performance--scalability)
8. [Distributed Architecture](#distributed-architecture)
9. [Plugin System Design](#plugin-system-design)
10. [Development Roadmap](#development-roadmap)

---

## Executive Summary

BlackMap v3.2 represents a quantum leap in reconnaissance capability, transforming the v3.0 modular architecture into a **production-grade, enterprise-ready framework** for network security assessment. The system maintains C and Rust as the sole implementation languages, ensuring:

- **Performance**: 20,000+ concurrent connections per node
- **Stealth**: Multi-dimensional evasion with adaptive behavior
- **Intelligence**: Real-time adaptive scanning with ML-ready metrics
- **Scalability**: Distributed controller/agent architecture for multi-node campaigns
- **Extensibility**: Plugin system for custom probes and fingerprinting databases
- **Safety**: Memory-safe C practices + Rust-based analysis with zero-copy FFI

**Key Enhancements:**
| Area | v3.0 | v3.2 |
|------|------|------|
| Max Concurrency | 256 | 20,000+ |
| Stealth Profiles | 5 | 12 (5 presets + 7 customizable variants) |
| Services Detectable | 10 | 30+ |
| Distributed Scanning | Single-node | Multi-node controller/agent |
| Metrics Export | JSON, CSV | JSON, Prometheus, SQLite |
| Fingerprint DB | Embedded | External + plugins |
| Adaptive Behavior | Basic | Advanced (RTT, packet loss, response patterns) |

---

## System Overview

### High-Level Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│                      CLI / Profile Manager                        │
│                                                                  │
│  ├─ Local Single-Node Scanning                                  │
│  ├─ Distributed Campaign Controller                              │
│  └─ Results Aggregation & Reporting                              │
└──────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴──────────────┐
                │                            │
        ┌───────▼────────────┐    ┌─────────▼──────────┐
        │ LOCAL SCANNER      │    │  DISTRIBUTED MODE  │
        │ (Orchestrator)     │    │                    │
        │                    │    │  Controller        │
        │ ┌────────────────┐ │    │  ├─ Task Mgmt      │
        │ │ Scheduler      │ │    │  ├─ Result Agg     │
        │ │ (Adaptive)     │ │    │  └─ Metric Coll    │
        │ └────────────────┘ │    │                    │
        │                    │    └────┬───────────────┘
        │ ┌────────────────┐ │         │
        │ │ Network Engine │ │    ┌────▼──────────┐
        │ │ (io_uring      │◄─────┤ Agent Pool    │
        │ │  optimized)    │ │    │ (N instances) │
        │ └─────┬──────────┘ │    └───────────────┘
        │       │            │
        │ ┌─────▼──────────┐ │
        │ │ Stealth System │ │
        │ │ (Advanced)     │ │
        │ └────────────────┘ │
        │                    │
        │ ┌────────────────┐ │
        │ │ Rust Analysis  │ │
        │ │ (Service FP)   │ │
        │ └────────────────┘ │
        │                    │
        │ ┌────────────────┐ │
        │ │ Metrics Engine │ │
        │ │ (Time-series)  │ │
        │ └────────────────┘ │
        │                    │
        │ ┌────────────────┐ │
        │ │ Plugin Manager │ │
        │ └────────────────┘ │
        └────────────────────┘
```

### Data Flow (Single-Node)

```
User Target Specification
    │ (IP, port, stealth level, detection, custom profiles)
    │
    ├─→ [Scan Plan Generation]
    │   ├─ IP Expansion (CIDR, ranges)
    │   ├─ Port List Assembly
    │   ├─ Stealth Profile Selection
    │   └─ Service Module Loading
    │
    ├─→ [Adaptive Scheduler]
    │   ├─ Task Queue Initialization
    │   ├─ Load Forecasting
    │   └─ Priority Assignment
    │
    ├─→ [Network Engine (epoll/io_uring)]
    │   ├─ Non-blocking socket creation
    │   ├─ Connection state machine
    │   ├─ Banner grabbing via recv()
    │   └─ RTT measurement (CLOCK_MONOTONIC)
    │
    ├─→ [Stealth System]
    │   ├─ Pre-connect delays (port randomization)
    │   ├─ Burst control (concurrency limiting)
    │   ├─ Adaptive jitter (RTT-aware)
    │   └─ Backoff on rate-limiting detection
    │
    ├─→ [Rust Analysis Engine (FFI)]
    │   ├─ Banner ↔ CStr marshaling
    │   ├─ Pattern matching (loaded databases)
    │   ├─ Version extraction (regex + heuristics)
    │   ├─ Confidence scoring
    │   └─ Plugin probe invocation
    │
    ├─→ [Metrics Collection]
    │   ├─ Per-connection stats (RTT, attempts)
    │   ├─ Per-port aggregates
    │   ├─ Per-service summaries
    │   └─ Time-series snapshots
    │
    └─→ [Output Formatting]
        ├─ JSON export
        ├─ Prometheus scrape endpoint
        ├─ SQLite persistence
        └─ Human-readable report
```

---

## Core Architecture Improvements

### 1. Advanced Stealth & Evasion System v3.2

#### 1.1 Architecture Overview

The v3.2 stealth system moves beyond basic timing control to a **multi-dimensional evasion framework**:

| Dimension | v3.0 | v3.2 |
|-----------|------|------|
| **Temporal** | Basic delays | Adaptive jitter + rate-limiting detection |
| **Spatial** | Random port order | Clustered scanning, decoy hosts, fragmentation |
| **Pattern** | Uniform distribution | Behavioral profiles (slow/aggressive/stealthy) |
| **Adaptive** | Manual RTT awareness | Automatic response to network feedback |

#### 1.2 Stealth Profiles (12 Total)

**5 Preset Profiles** (compatible with v3.0):

```c
typedef enum {
    STEALTH_PERFORMANCE       = 0,      // 256 global CC, 0% jitter
    STEALTH_BALANCED          = 1,      // 128 global CC, 10% jitter
    STEALTH_LOW_NOISE         = 2,      // 32 global CC, 25% jitter
    STEALTH_CONSERVATIVE      = 3,      // 8 global CC, 50% jitter
    STEALTH_ULTRA_CONSERVATIVE = 4      // 1 global CC, 80% jitter
} stealth_preset_t;
```

**7 New Specialized Variants** (customization):

```c
typedef enum {
    STEALTH_DECOY             = 5,      // Intermix fake sources
    STEALTH_FRAGMENTS         = 6,      // Packet fragmentation
    STEALTH_TIMING_NOISE      = 7,      // Extreme temporal randomization
    STEALTH_SLOW_CREEP        = 8,      // Minimal observable activity
    STEALTH_RANDOMIZED_GAPS   = 9,      // Variable inter-packet gaps
    STEALTH_TRAFFIC_SHAPING   = 10,     // Mimics legitimate traffic
    STEALTH_CUSTOM_PROFILE    = 11      // User-defined blending
} stealth_variant_t;
```

#### 1.3 Advanced Stealth Features

**A. Packet Fragmentation Module**

```c
typedef struct {
    uint8_t enabled;
    uint16_t fragment_size;      // 20-1500 bytes
    uint8_t randomize_size;      // 0=fixed, 1=within ±20%, 2=within ±50%
    uint32_t inter_fragment_delay_us;
} fragmentation_config_t;

typedef struct {
    uint32_t original_packet_id;
    uint8_t fragment_offset;   // 0, 1, 2, ... N
    uint8_t more_fragments;    // Boolean
    uint8_t data[2048];
    uint32_t data_len;
} raw_packet_fragment_t;
```

**B. Decoy Host Simulation**

```c
typedef struct {
    char **decoy_ips;           // Array of decoy IPs to intersperse
    uint32_t decoy_count;
    uint8_t decoy_pattern;      // 0=random, 1=round-robin, 2=clustered
    uint8_t decoy_real_ratio;   // 70 = 70% real targets, 30% decoys
    uint8_t use_spoofed_macs;   // Generate fake source MACs
} decoy_config_t;
```

**C. Adaptive Timing Jitter**

```c
typedef struct {
    uint32_t base_delay_us;
    uint8_t jitter_distribution;  // 0=uniform, 1=exponential, 2=poisson
    float jitter_coefficient;      // 0.0-1.0
    
    // Adaptive components
    uint8_t adapt_to_rtt;
    uint8_t adapt_to_loss;
    uint8_t adapt_to_timeouts;
    
    // State tracking
    uint64_t last_adjustment_time_us;
    float adaptive_multiplier;     // Dynamically adjusted
} adaptive_timing_t;
```

**D. Rate-Limiting Detection & Response**

```c
typedef struct {
    uint8_t enabled;
    uint32_t measurement_window_ms;    // 1000ms window
    uint32_t error_rate_threshold;     // 30% = triggers backoff
    uint32_t timeout_spike_threshold;  // 3x normal RTT = rate limit
    
    // Backoff strategy
    float backoff_multiplier;          // 1.5x, 2.0x, etc.
    uint32_t max_backoff_delay_ms;     // Cap (e.g., 30s)
    uint8_t exponential_backoff;       // Increase wait geometrically
} rate_limit_detection_t;
```

#### 1.4 New CLI Options

```bash
# Fragmentation
--fragment                           # Enable basic fragmentation
--fragment-size <bytes>              # Fragment size (20-1500)
--fragment-randomize <pct>           # Size randomization (0, 20, 50)
--fragment-delay <us>                # Inter-fragment delay (microseconds)

# Decoys
--decoy <ip,ip,ip>                   # Comma-separated decoy IPs
--decoy-ratio <pct>                  # Percentage real vs decoy (default: 100)
--decoy-pattern <random|round|cluster>

# Advanced Timing
--jitter-distribution <uniform|exp|poisson>
--jitter-coefficient <0.0-1.0>       # Jitter intensity
--adapt-to-rtt                       # Auto-adjust timing based on RTT
--adapt-to-loss                      # Auto-adjust based on packet loss

# Rate Limiting
--detect-rate-limit                  # Enable rate-limit detection
--rate-limit-threshold <pct>         # Error rate threshold (default: 30)
--backoff-multiplier <1.5-3.0>       # Geometric backoff factor
--max-backoff <ms>                   # Backoff ceiling

# Traffic Shaping
--traffic-shape <profile>            # 0=random, 1=http-like, 2=dns-like
--max-rate <pps>                     # Packets per second (hard limit)
--scan-delay <ms>                    # Delay between probes
--inter-host-delay <ms>              # Pause when switching targets

# Custom Profile
--stealth-level <0-4>                # Preset levels (0-4)
--stealth-profile <name>             # Custom profile name
--custom-stealth-file <json>         # JSON config for full customization
```

#### 1.5 Stealth Configuration Data Structure

```c
typedef struct {
    // Identity
    stealth_preset_t preset;
    stealth_variant_t variant;
    char profile_name[256];
    
    // Fragmentation
    fragmentation_config_t fragmentation;
    
    // Decoys
    decoy_config_t decoys;
    
    // Timing
    adaptive_timing_t adaptive_timing;
    uint32_t pre_connect_delay_us;
    uint32_t post_connect_delay_us;
    uint32_t inter_port_delay_ms;
    uint32_t inter_host_delay_ms;
    
    // Rate Control
    uint32_t max_packets_per_second;
    uint32_t burst_size;
    uint32_t pause_after_burst_ms;
    
    // Rate Limiting Detection
    rate_limit_detection_t rate_limit_detection;
    
    // Concurrency
    uint32_t max_global_concurrency;
    uint32_t max_per_host_concurrency;
    
    // Port/Host Randomization
    uint8_t randomize_ports;
    uint8_t randomize_hosts;
    uint8_t randomize_port_order_per_host;  // NEW
    
    // Version Detection
    uint8_t skip_version_detection;
    uint32_t version_detection_timeout_ms;
    uint8_t defer_version_detection;        // NEW: detect in post-phase
    
    // Response Tracking
    uint64_t rtt_history[256];              // Circular buffer of recent RTTs
    uint32_t rtt_history_idx;
    uint32_t timeout_count;
    uint32_t error_count;
    uint64_t last_adjustment_time_us;
} stealth_config_v32_t;
```

---

### 2. Adaptive Scanning Engine (ASE)

#### 2.1 Architecture Overview

The Adaptive Scanning Engine monitors real-time network feedback and automatically adjusts scanning behavior without user intervention.

**Design Principle**: *The network is not static; responses change with time and conditions.*

#### 2.2 Feedback Channels

```c
typedef struct {
    // Per-host RTT statistics
    uint64_t rtt_min_us;
    uint64_t rtt_avg_us;
    uint64_t rtt_max_us;
    uint32_t rtt_stddev_us;
    
    // Packet loss estimation
    uint32_t packets_sent;
    uint32_t packets_timeout;
    float packet_loss_percent;
    
    // Timeout patterns
    uint32_t consecutive_timeouts;
    uint32_t timeout_begin_timestamp_us;
    
    // Response patterns
    uint32_t responsive_ports;
    uint32_t filtered_ports;
    uint32_t refused_connections;
    
    // Behavioral indicators
    uint8_t likely_rate_limited;
    uint8_t likely_dos_protection;
    uint8_t likely_firewall_drop;
    uint8_t likely_firewall_reject;
} network_feedback_t;
```

#### 2.3 Adaptive Mechanisms

**A. Concurrency Adjustment**

```c
typedef struct {
    uint32_t base_concurrency;
    uint32_t current_concurrency;
    float rtt_variation_factor;        // RTT stddev / RTT avg
    uint32_t adjustment_interval_ms;   // Check every 500ms
    
    // Rules
    uint8_t increase_on_low_rtt_variation;
    uint8_t decrease_on_timeout_spikes;
    float aggressive_increase_factor;   // 1.2x when conditions improve
    float conservative_decrease_factor; // 0.7x when conditions worsen
} concurrency_adapter_t;
```

**B. Timeout Recalculation**

```c
typedef struct {
    uint32_t base_timeout_ms;
    uint32_t current_timeout_ms;
    
    // Calculation: current_timeout = base * (1 + RTT_factor)
    // where RTT_factor = (observed_rtt - min_rtt) / min_rtt
    float rtt_safety_margin;           // 1.5x = 50% extra on observed RTT
    
    uint32_t min_timeout_ms;
    uint32_t max_timeout_ms;
} timeout_adapter_t;
```

**C. Retry Strategy Adaptation**

```c
typedef struct {
    uint32_t base_retries;
    uint32_t current_retries;
    
    // Backoff when errors detected
    uint8_t enable_exponential_backoff;
    uint32_t backoff_base_ms;
    float backoff_exponent;            // 2.0 = exponential doubling
    
    // Vary retry based on RTT variance
    uint8_t increase_retries_on_variance;
} retry_adapter_t;
```

#### 2.4 Adaptive State Machine

```
PHASE 0: INITIAL SCAN
  - Use base parameters
  - Collect first 50 measurements
  - Establish baseline RTT, loss rate

       ↓

PHASE 1: ANALYSIS
  - Compute statistics
  - Detect rate-limiting / firewall signals
  - Evaluate network stability

       ↓

PHASE 2: ADJUSTMENT
  if (rtt_variance < 10%):
    increase_concurrency(20%)       # Network is stable
  
  if (packet_loss > 5%):
    increase_timeout(30%)
    decrease_concurrency(15%)
  
  if (consecutive_timeouts > 3):
    enable_exponential_backoff()
    adjust_timing(slower)

       ↓

PHASE 3: CONTINUED MONITORING
  - During scan, periodically re-evaluate
  - Apply incremental adjustments
  - Log adaptation events
  
       ↓ (every 1000 tasks or 5 seconds)

PHASE 4: REFINEMENT
  - Fine-tune based on current performance
  - Never exceed user-specified limits
```

#### 2.5 Adaptive Engine API

```c
typedef struct {
    network_feedback_t feedback;
    concurrency_adapter_t concurrency;
    timeout_adapter_t timeout;
    retry_adapter_t retry;
    
    // Measurement tracking
    uint32_t measurements_collected;
    uint64_t baseline_rtt_us;
    
    // State
    uint8_t adaptation_phase;
    uint64_t last_adjustment_time_us;
    uint32_t adjustment_count;          // Track how many times we've adjusted
} adaptive_engine_t;

// API
adaptive_engine_t* adaptive_engine_init(
    uint32_t base_concurrency,
    uint32_t base_timeout_ms,
    uint32_t base_retries
);

void adaptive_engine_add_measurement(
    adaptive_engine_t *ae,
    uint64_t rtt_us,
    uint8_t success,
    uint8_t timeout
);

typedef struct {
    uint32_t recommended_concurrency;
    uint32_t recommended_timeout_ms;
    uint32_t recommended_retries;
    uint8_t should_backoff;
    char reason[512];                  // Human-readable explanation
} adaptation_t;

adaptation_t adaptive_engine_get_recommendations(
    adaptive_engine_t *ae
);

void adaptive_engine_apply_adjustment(
    adaptive_engine_t *ae,
    const adaptation_t *adj
);

void adaptive_engine_free(adaptive_engine_t *ae);
```

---

### 3. Rust-Based Service Fingerprinting Engine (RSFE)

#### 3.1 Architecture & Design

The Fingerprinting Engine is the intelligence layer that transforms raw banner data into actionable service intelligence.

**Key Features:**
- **Modular Database**: Services defined in TOML/JSON, loaded at runtime
- **Protocol-Aware**: Understands banner formats for specific protocols
- **Confidence Scoring**: Multi-factor scoring (exact match, pattern match, heuristic)
- **Plugin Integration**: External probes can register new detectors

#### 3.2 Fingerprint Database Structure

**File**: `data/fingerprints.toml`

```toml
# HTTP/HTTPS Services
[[services.http]]
name = "Apache"
port = 80
protocol = "HTTP"
confidence_threshold = 85

[[services.http.patterns]]
type = "header"          # type: banner, header, status_code, version string
pattern = "Server: Apache/([0-9.]+)"
extract_version = true
confidence_boost = 20    # Additional % if matched

[[services.http.patterns]]
type = "header"
pattern = "X-Powered-By:.*PHP"
extract_version = false
confidence_boost = 10

[[services.http.heuristics]]
rule = "server_header_contains_date AND http_version == 1.1"
confidence_boost = 5

# SSH Services
[[services.ssh]]
name = "OpenSSH"
port = 22
protocol = "SSH"
confidence_threshold = 90

[[services.ssh.patterns]]
type = "banner"
pattern = "SSH-2\\.0-OpenSSH_([0-9.]+[a-zA-Z0-9]*)"
extract_version = true
version_cleanup = "remove_p_suffix"   # OpenSSH_7.4p1 -> 7.4
confidence_boost = 30

[[services.ssh.version_signatures]]
version = "7.4p1"
os = "Ubuntu 18.04 / Debian 9"
release_date = "2018-04-21"
eol_date = "2019-01-10"
cve_list = ["CVE-2018-15473"]

# Database Services
[[services.mysql]]
name = "MySQL"
port = 3306
protocol = "MySQL"

[[services.mysql.patterns]]
type = "banner"
pattern = "([0-9]+\\.[0-9]+\\.[0-9]+[a-zA-Z0-9\\-]*)-MySQL"
extract_version = true

# Custom fingerprint rules
[[custom_signatures]]
name = "web_framework_detection"
module = "plugins/web_framework.rs"
priority = 100
```

#### 3.3 Fingerprint Data Structure (Rust)

```rust
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct ServiceFingerprint {
    pub service: String,           // "Apache", "OpenSSH", etc.
    pub product: String,           // "Apache HTTP Server", "OpenSSH"
    pub version: Option<String>,
    pub version_details: Option<VersionDetails>,
    pub confidence: u8,            // 0-100
    pub detected_via: String,      // "banner_match", "heuristic", "plugin"
    pub metadata: HashMap<String, String>,
    pub cpe: Option<String>,       // CPE identifier for vuln mapping
    pub os_info: Option<OSInfo>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct VersionDetails {
    pub version_string: String,
    pub release_date: Option<String>,
    pub eol_date: Option<String>,
    pub known_cves: Vec<String>,
    pub known_vulnerabilities: Vec<Vulnerability>,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct OSInfo {
    pub detected_os: String,
    pub os_family: String,             // Windows, Linux, BSD, etc.
    pub confidence: u8,
}

#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct Vulnerability {
    pub cve_id: String,
    pub cvss_score: f32,
    pub description: String,
}
```

#### 3.4 Pattern Matching Engine

```rust
pub trait PatternMatcher {
    fn match_banner(&self, banner: &[u8]) -> Option<ServiceFingerprint>;
}

// Implementation examples:
pub struct RegexMatcher {
    service: String,
    patterns: Vec<(Regex, u8)>,  // (pattern, confidence_boost)
}

pub struct StructuredMatcher {
    service: String,
    protocol: Protocol,  // HTTP, SSH, FTP, MySQL, etc.
    version_extractor: Box<dyn Fn(&str) -> Option<String>>,
}

pub enum Protocol {
    Http,
    Https,
    Ssh,
    Ftp,
    Smtp,
    Dns,
    Mysql,
    Postgresql,
    Redis,
    Mongodb,
    Custom(String),
}
```

#### 3.5 30+ Supported Services

```
HTTP/HTTPS:
  - Apache (HTTP Server)
  - Nginx (HTTP Server)
  - IIS (Microsoft Internet Information Services)
  - Lighttpd (HTTP Server)
  - Cherokee (HTTP Server)
  - Hiawatha (HTTP Server)
  - Jetty (Java Web Server)
  - Tomcat (Java Servlet Container)

SSH:
  - OpenSSH
  - libssh
  - Dropbear (Embedded SSH)
  - PuTTY SSH Server

FTP:
  - vsftpd (Very Secure FTP Daemon)
  - ProFTPD
  - Pure-FTPd
  - Wu-FTPd
  - FileZilla Server

SMTP/Mail:
  - Postfix
  - Sendmail
  - Exim
  - qmail

DNS:
  - BIND (ISC BIND)
  - dnsmasq
  - PowerDNS
  - CoreDNS
  - Unbound

Databases:
  - MySQL / MariaDB
  - PostgreSQL
  - MongoDB
  - Redis
  - Cassandra
  - Memcached

Remote Access:
  - RDP (Remote Desktop Protocol)
  - VNC (Virtual Network Computing)
  - Telnet

Others:
  - Kerberos (MIT, Heimdal)
  - LDAP (OpenLDAP, Active Directory)
  - Syslog
```

#### 3.6 FFI Boundary (C ↔ Rust)

```c
/* C Header: include/blackmap3/analysis.h */

typedef struct {
    char service[128];              // "Apache", "OpenSSH"
    char product[256];              // "Apache HTTP Server"
    char version[64];               // "2.4.41"
    uint8_t confidence;             // 0-100
    char detected_via[64];          // "banner_match", "heuristic"
    char cpe[256];                  // CPE identifier
    
    // Extended metadata
    char **metadata_keys;           // NULL-terminated array
    char **metadata_values;
    uint32_t metadata_count;
} service_fingerprint_c_t;

// Rust returns opaque pointer
service_fingerprint_c_t* analysis_fingerprint_banner(
    const uint8_t *banner,
    uint32_t banner_len,
    uint16_t source_port
);

// Rust returns JSON string (C frees with free())
char* analysis_fingerprint_to_json(
    const service_fingerprint_c_t *fp
);

// Load custom fingerprint DB
int analysis_load_fingerprint_db(const char *db_path);

// Free fingerprint
void analysis_free_fingerprint(service_fingerprint_c_t *fp);
```

#### 3.7 Service Detection Output Example

```json
{
  "host": "192.168.1.100",
  "port": 80,
  "service": "HTTP",
  "fingerprints": [
    {
      "product": "Apache HTTP Server",
      "version": "2.4.41",
      "os_info": {
        "detected_os": "Ubuntu 18.04 LTS",
        "os_family": "Linux",
        "confidence": 75
      },
      "confidence": 95,
      "detected_via": "banner_match",
      "cpe": "cpe:/a:apache:http_server:2.4.41",
      "version_details": {
        "release_date": "2019-03-28",
        "eol_date": "2020-12-01",
        "known_cves": ["CVE-2019-0211", "CVE-2019-0821"]
      },
      "metadata": {
        "modules": "mod_ssl,mod_rewrite,mod_deflate",
        "ssl_version": "TLSv1.2"
      }
    }
  ]
}
```

---

### 4. Enhanced Metrics & Telemetry Engine

#### 4.1 Metrics Categories

**Connection-Level Metrics:**
```c
typedef struct {
    uint32_t connection_id;
    char target_ip[46];              // IPv4 + IPv6 support
    uint16_t target_port;
    
    uint64_t connect_start_time_us;
    uint64_t connect_end_time_us;
    uint64_t rtt_us;
    
    uint8_t state_final;             // enum: OPEN, CLOSED, FILTERED, TIMEOUT
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t retry_count;
    
    char banner[4096];               // Banner data
} connection_metric_t;
```

**Port-Level Aggregates:**
```c
typedef struct {
    uint16_t port;
    char protocol[32];               // "TCP", "UDP", etc.
    
    // Latency distribution
    uint32_t sample_count;
    uint64_t rtt_min_us;
    uint64_t rtt_avg_us;
    uint64_t rtt_p50_us, rtt_p90_us, rtt_p99_us;
    uint64_t rtt_max_us;
    
    // Success metrics
    uint32_t open_count;
    uint32_t closed_count;
    uint32_t filtered_count;
    uint32_t timeout_count;
    
    // Service detection
    service_fingerprint_c_t detected_service;
    uint8_t version_detected;
    float detection_confidence_avg;
} port_metrics_t;
```

**Service-Level Aggregates:**
```c
typedef struct {
    char service_name[128];
    char product[256];
    uint32_t instances_detected;
    uint32_t versions[32];           // Distribution of detected versions
    uint8_t most_common_version_confidence;
    
    // Vulnerability tracking
    uint32_t cve_count;
    float avg_cvss_score;
} service_metrics_t;
```

**Scan-Level Summary:**
```c
typedef struct {
    uint64_t scan_start_time_us;
    uint64_t scan_end_time_us;
    uint32_t scan_duration_us;
    
    // Global statistics
    uint32_t targets_scanned;
    uint32_t total_ports_scanned;
    uint32_t total_connections_attempted;
    uint32_t total_connections_open;
    
    // Performance
    float throughput_connections_per_sec;
    float avg_connection_duration_us;
    float peak_concurrent_connections;
    
    // Efficiency
    uint32_t total_timeouts;
    uint32_t total_retries;
    float overall_success_rate;
    
    // Service detection
    uint32_t unique_services_detected;
    uint32_t total_os_signatures;
    float average_detection_confidence;
    
    // Adaptive engine activity
    uint32_t concurrency_adjustments;
    uint32_t timeout_adjustments;
    uint32_t backoff_events;
} scan_summary_metrics_t;
```

#### 4.2 Time-Series Metrics

```c
typedef struct {
    uint64_t timestamp_us;
    uint32_t active_connections;
    uint32_t connections_completed;
    float throughput_pps;              // Packets per second
    float avg_rtt_us;
    float packet_loss_percent;
    uint32_t timeouts_in_window;
} metric_sample_t;

typedef struct {
    metric_sample_t *samples;          // Circular buffer
    uint32_t sample_count;
    uint32_t max_samples;              // Typically 3600 for 1-hour history
    uint32_t current_idx;
    uint32_t sample_interval_ms;       // Frequency (e.g., 100ms)
} time_series_metrics_t;
```

#### 4.3 Metrics Export Formats

**JSON Export:**
```json
{
  "scan_summary": {
    "duration_ms": 45230,
    "targets_scanned": 256,
    "total_ports_probed": 256000,
    "successful_connections": 1240,
    "detection_rate": 0.82,
    "average_detection_confidence": 92.3
  },
  "per_service": {
    "HTTP": {
      "instances": 42,
      "versions": {"2.4.41": 25, "2.4.39": 17},
      "most_common": "2.4.41"
    },
    "SSH": {
      "instances": 256,
      "versions": {"7.4p1": 128, "6.6p1": 128}
    }
  },
  "time_series_sample": [
    {"timestamp": "2026-03-05T14:30:00Z", "connections": 128, "throughput_pps": 1240},
    {"timestamp": "2026-03-05T14:30:01Z", "connections": 140, "throughput_pps": 1310}
  ]
}
```

**SQLite Schema:**
```sql
CREATE TABLE scan_metadata (
    scan_id TEXT PRIMARY KEY,
    scan_start_time INTEGER,
    scan_end_time INTEGER,
    stealth_profile TEXT,
    target_count INTEGER,
    port_count INTEGER
);

CREATE TABLE connections (
    connection_id INTEGER PRIMARY KEY,
    scan_id TEXT,
    target_ip TEXT,
    target_port INTEGER,
    rtt_us INTEGER,
    state TEXT,
    service TEXT,
    version TEXT,
    confidence INTEGER,
    timestamp INTEGER,
    FOREIGN KEY(scan_id) REFERENCES scan_metadata(scan_id)
);

CREATE INDEX idx_by_service ON connections(service);
CREATE INDEX idx_by_state ON connections(state);
CREATE INDEX idx_by_target ON connections(target_ip);
```

**Prometheus Format:**
```
# HELP blackmap_scan_duration_seconds Duration of network scan in seconds
# TYPE blackmap_scan_duration_seconds gauge
blackmap_scan_duration_seconds{stealth_profile="balanced"} 45.230

# HELP blackmap_connections_successful Total successful connections
# TYPE blackmap_connections_successful counter
blackmap_connections_successful{stealth_profile="balanced"} 1240

# HELP blackmap_rtt_ms RTT distribution in milliseconds
# TYPE blackmap_rtt_ms histogram
blackmap_rtt_ms_bucket{le="1"} 100
blackmap_rtt_ms_bucket{le="5"} 450
blackmap_rtt_ms_bucket{le="10"} 890
blackmap_rtt_ms_bucket{le="50"} 1200
blackmap_rtt_ms_bucket{le="+Inf"} 1240

# HELP blackmap_services_detected Number of unique services detected
# TYPE blackmap_services_detected gauge
blackmap_services_detected 8
```

---

### 5. Distributed Scanning Architecture

#### 5.1 System Design

For large-scale reconnaissance campaigns across multiple network segments, v3.2 introduces an optional **distributed controller-agent model**:

```
┌──────────────────────────────┐
│ Controller (single instance)  │
│  ├─ Scan Plan Generator      │
│  ├─ Task Coordinator          │
│  ├─ Results Aggregator        │
│  └─ Metrics Collector         │
└──────────┬───────────────────┘
           │ (TCP/JSON-RPC or AMQP)
      ┌────┴────┬──────┬──────┐
      │          │      │      │
   ┌──▼──┐   ┌──▼──┐ ┌──▼──┐ ┌──▼──┐
   │Agent│   │Agent│ │Agent│ │Agent│
   │ 1   │   │ 2   │ │ 3   │ │ N   │
   └──────┘   └──────┘ └──────┘ └──────┘
      │         │        │        │
   Exec scans  Rescan  Report  Collect
   (1/4 targets)
```

#### 5.2 Controller Responsibilities

```c
typedef struct {
    // Configuration
    char **agent_endpoints;            // Array of "localhost:9000", etc.
    uint32_t agent_count;
    uint32_t max_tasks_per_agent;      // Load balancing

    // State management
    task_queue_t *task_queue;          // Master queue
    completed_result_t *results;       // Aggregated results
    uint32_t result_count;
    
    // Metrics
    scan_summary_metrics_t aggregated_metrics;
    agent_status_t *agent_statuses;
} distributed_controller_t;

// API
distributed_controller_t* distributed_controller_init(
    const char **agent_endpoints,
    uint32_t agent_count
);

int distributed_controller_distribute_tasks(
    distributed_controller_t *ctrl,
    const task_t *tasks,
    uint32_t task_count
);

typedef struct {
    uint32_t agent_id;
    uint32_t tasks_assigned;
    uint32_t tasks_completed;
    uint32_t tasks_failed;
    uint64_t last_heartbeat_us;
    uint8_t is_healthy;
} agent_status_t;

agent_status_t* distributed_controller_get_agent_status(
    distributed_controller_t *ctrl,
    uint32_t agent_id
);

void distributed_controller_free(distributed_controller_t *ctrl);
```

#### 5.3 Agent Responsibilities

```c
typedef struct {
    uint32_t agent_id;
    char controller_endpoint[256];     // "localhost:9000"
    
    // Local scanner
    blackmap_t *local_scanner;
    
    // Task reception
    task_queue_t *pending_tasks;
    
    // Execution
    uint8_t is_running;
    uint64_t last_heartbeat_us;
} distributed_agent_t;

// API
distributed_agent_t* distributed_agent_init(
    uint32_t agent_id,
    const char *controller_endpoint
);

int distributed_agent_run(distributed_agent_t *agent);
```

#### 5.4 Communication Protocol

**JSON-RPC over TCP (recommended for simplicity)**

```json
// Controller -> Agent: Assign tasks
{
  "jsonrpc": "2.0",
  "method": "agent.execute_tasks",
  "params": {
    "scan_id": "scan_2026_03_05_001",
    "tasks": [
      {
        "target_ip": "192.168.1.1",
        "ports": [22, 80, 443, 3306],
        "stealth_profile": "balanced",
        "service_detection": true
      },
      {
        "target_ip": "192.168.1.2",
        "ports": [22, 80, 443, 3306],
        "stealth_profile": "balanced",
        "service_detection": true
      }
    ]
  },
  "id": 1
}

// Agent -> Controller: Task completion
{
  "jsonrpc": "2.0",
  "method": "controller.report_progress",
  "params": {
    "scan_id": "scan_2026_03_05_001",
    "agent_id": 1,
    "completed_tasks": 2,
    "failed_tasks": 0,
    "results": [
      {
        "target_ip": "192.168.1.1",
        "open_ports": [22, 80, 443],
        "services": {...}
      }
    ]
  },
  "id": 1
}
```

---

### 6. Performance Improvements

#### 6.1 io_uring Backend (Optional)

Linux io_uring provides superior performance for high-concurrency I/O on modern kernels (5.1+):

```c
typedef enum {
    IO_ENGINE_SELECT    = 0,           // Maximum compatibility
    IO_ENGINE_EPOLL     = 1,           // Linux standard
    IO_ENGINE_IOURING   = 2            // High-performance (Linux 5.1+)
} io_engine_type_t;

typedef struct {
    io_engine_type_t engine_type;
    
    // For io_uring
    unsigned int ring_size;            // Submission queue size (e.g., 1024)
    unsigned int flags;                // IORING_SETUP_* flags
    int cqs_shared;                    // Completion queue shared
    
    // Performance tuning
    unsigned int max_concurrent_ops;
    unsigned int batch_submit_size;    // Submit in batches
} io_engine_config_t;

typedef struct {
    io_engine_type_t type;
    
    // For io_uring
    struct io_uring *ring;             // Ring handle
    
    // Connection tracking
    connection_t *connections;
    uint32_t max_connections;
} io_engine_ctx_t;

// Configuration
io_engine_ctx_t* io_engine_init(const io_engine_config_t *config);

// Event loop
int io_engine_submit_connect(io_engine_ctx_t *ctx, connection_t *conn);
int io_engine_process_events(io_engine_ctx_t *ctx, uint32_t timeout_ms);
```

**Performance Expectations:**

| Metric | epoll | io_uring |
|--------|-------|----------|
| Syscalls/sec | ~1000 | ~50 |
| Latency (p50 RTT) | 15ms | 13ms |
| Memory per conn | 4KB | 3.5KB |
| Max concurrency | 10K | 20K+ |

#### 6.2 Buffer Pooling Optimization

```c
typedef struct {
    uint8_t **buffers;
    uint32_t buffer_size;
    uint32_t pool_size;
    uint32_t allocated;
    
    // Statistics
    uint64_t allocations;
    uint64_t deallocations;
    uint64_t pool_misses;              // Requests when pool empty
} buffer_pool_t;

buffer_pool_t* buffer_pool_init(
    uint32_t pool_size,
    uint32_t buffer_size
);

uint8_t* buffer_pool_acquire(buffer_pool_t *pool);
int buffer_pool_release(buffer_pool_t *pool, uint8_t *buffer);

// Pre-allocate on startup
int buffer_pool_warm(buffer_pool_t *pool);
```

#### 6.3 Connection Reuse (HTTP Keep-Alive)

```c
typedef struct {
    int reuse_connections;             // Enable keep-alive
    uint32_t keep_alive_timeout_ms;    // 30000 = 30 seconds
    uint32_t max_reuses_per_connection;// 100 = reuse up to 100 times
} connection_reuse_config_t;

// When HTTP Connection: keep-alive is detected,
// mark connection as reusable rather than closing
int connection_mark_reusable(connection_t *conn);
int connection_reuse(connection_t *conn, const char *new_path);
```

#### 6.4 Memory Optimization

**Per-Connection Memory Profile (v3.2 Target):**

```
Connection Struct:            256 bytes
State Machine:                64 bytes
Buffers (2x read,1x write):   6144 bytes (6KB total)
Timer/RTT tracking:           64 bytes
Stealth tracking:             32 bytes
─────────────────────────────────────
Total per connection:         ~6.5 KB

For 20,000 connections:       ~130 MB (very reasonable)
```

**Memory Pooling:**

```c
// Allocate large contiguous block upfront
void* connection_pool = malloc(20000 * sizeof(connection_t));

// Use bump allocator or free-list for allocation
typedef struct {
    void *pool_base;
    void *pool_top;
    uint32_t pool_size;
    connection_t *freelist;
} connection_pool_allocator_t;
```

---

### 7. Plugin System Architecture

#### 7.1 Plugin Types

**A. Protocol Probe Plugins**

```c
typedef struct {
    uint16_t target_port;
    char probe_name[256];              // "custom_ssh_probe"
    
    // Probe execution
    int (*probe_fn)(
        int sock,
        const uint8_t *received_banner,
        uint32_t banner_len,
        probe_result_t *result
    );
} protocol_probe_plugin_t;

typedef struct {
    uint8_t service_detected;
    char service[128];
    char version[64];
    uint8_t confidence;
    char metadata[1024];
} probe_result_t;
```

**B. Fingerprint Database Plugins**

```c
typedef struct {
    char db_name[256];
    char db_version[64];
    
    // Dynamic loading
    int (*load_db_fn)(const char *db_path);
    int (*match_fn)(
        const uint8_t *banner,
        uint32_t banner_len,
        service_fingerprint_c_t *result
    );
    void (*unload_db_fn)(void);
} fingerprint_db_plugin_t;
```

**C. Custom Scan Technique Plugins**

```c
typedef struct {
    char technique_name[256];          // "syn_stealth_fragment"
    
    // Technique implementation
    int (*init_fn)(void);
    int (*execute_fn)(
        int target_sock,
        const char *target_ip,
        uint16_t target_port,
        scan_result_t *result
    );
    void (*cleanup_fn)(void);
} scan_technique_plugin_t;
```

#### 7.2 Plugin Registration System

```c
typedef struct {
    plugin_type_t type;                // PROBE, DB, TECHNIQUE
    char plugin_path[512];             // "/opt/blackmap-plugins/custom_ssh.so"
    void *handle;                       // dlopen() handle
    plugin_metadata_t metadata;
} loaded_plugin_t;

typedef struct {
    loaded_plugin_t *plugins;
    uint32_t plugin_count;
} plugin_manager_t;

// API
plugin_manager_t* plugin_manager_init(void);

int plugin_manager_load(
    plugin_manager_t *pm,
    const char *plugin_path
);

int plugin_manager_execute_probes(
    plugin_manager_t *pm,
    int sock,
    const uint8_t *banner,
    uint32_t banner_len,
    service_fingerprint_c_t *result
);

void plugin_manager_free(plugin_manager_t *pm);
```

#### 7.3 Plugin Manifest (JSON)

```json
{
  "name": "CustomSSHProbe",
  "version": "1.0.0",
  "type": "protocol_probe",
  "author": "Security Team",
  "target_service": "SSH",
  "target_port": 22,
  "library": "libcustom_ssh_probe.so",
  "entry_point": "ssh_probe_main",
  "dependencies": [],
  "configuration": {
    "probe_timeout_ms": 5000,
    "max_banner_size": 4096
  }
}
```

#### 7.4 Plugin Development Example (C)

```c
// File: plugins/custom_ssh_probe.c

#include "../include/blackmap3/analysis.h"
#include <string.h>

typedef struct {
    int initialized;
} plugin_state_t;

static plugin_state_t state = {0};

// Entry point
int ssh_probe_main(int sock, const uint8_t *banner, uint32_t banner_len,
                   service_fingerprint_c_t *result) {
    if (!banner || banner_len < 4) {
        return -1;
    }

    // Custom SSH detection logic
    if (memcmp(banner, "SSH-", 4) != 0) {
        return -1;
    }

    // Extract version (custom implementation)
    const char *impl_start = (char*)banner + 4;
    const char *impl_end = strchr(impl_start, '\r');
    
    if (!impl_end) {
        impl_end = strchr(impl_start, '\n');
    }
    
    if (!impl_end) {
        impl_end = (char*)banner + banner_len;
    }

    // Populate result
    strncpy(result->service, "SSH", sizeof(result->service) - 1);
    strncpy(result->product, "OpenSSH", sizeof(result->product) - 1);
    result->confidence = 95;
    strncpy(result->detected_via, "custom_probe", sizeof(result->detected_via) - 1);

    return 0;
}
```

---

## Data Structures & API Design

### 1. Unified Scan Configuration

```c
typedef struct {
    // Target specification
    char **target_ips;
    uint32_t target_count;
    char **ports_spec;                 // "22,80,443", "1-1000", etc.
    uint16_t *ports;
    uint32_t port_count;
    
    // Scanning parameters
    io_engine_type_t io_engine;
    scan_type_t scan_type;             // CONNECT, SYN, UDP, etc.
    
    // Stealth configuration
    stealth_config_v32_t stealth;
    
    // Service detection
    uint8_t enable_service_detection;
    uint8_t enable_version_detection;
    uint8_t enable_os_detection;
    
    // Adaptive scanning
    uint8_t enable_adaptive_engine;
    adaptive_engine_config_t adaptive_config;
    
    // Output
    char output_format[64];            // "json", "csv", "table"
    char output_file[512];
    
    // Plugins
    plugin_manager_t *plugins;
    
    // Metrics
    uint8_t collect_detailed_metrics;
    metrics_export_format_t export_format;
    
    // Advanced features
    uint8_t enable_distributed;
    distributed_controller_t *controller;
} blackmap_scan_config_t;
```

### 2. Result Structures

```c
typedef struct {
    char host[46];                     // IPv4 or IPv6
    uint16_t port;
    port_state_t state;                // OPEN, CLOSED, FILTERED
    
    uint64_t rtt_us;
    service_fingerprint_c_t service;
    
    struct {
        char **banner_lines;
        uint32_t line_count;
    } raw_data;
} port_result_t;

typedef struct {
    char host[46];
    port_result_t *ports;
    uint32_t port_count;
    
    float scan_confidence;
} host_result_t;

typedef struct {
    host_result_t *hosts;
    uint32_t host_count;
    
    scan_summary_metrics_t metrics;
    time_series_metrics_t *time_series;
} scan_result_t;
```

---

## FFI Boundaries & Type Safety

### Memory Safety Principles

1. **No unsafe Rust in public API**: All FFI boundaries are wrapped in safe Rust
2. **CString validation**: Input strings validated before processing
3. **Buffer overflow protection**: All C->Rust transmissions use length fields
4. **Reference counting**: Rust tracks C allocations for cleanup

### Safe FFI Pattern

```rust
// Rust side (safe wrapper)
#[no_mangle]
pub extern "C" fn analysis_fingerprint_banner(
    banner: *const u8,
    banner_len: u32,
    port: u16,
) -> *mut service_fingerprint_c_t {
    // Validate input
    if banner.is_null() || banner_len == 0 || banner_len > 65536 {
        return std::ptr::null_mut();
    }

    // Convert to safe Rust
    let banner_slice = unsafe {
        std::slice::from_raw_parts(banner, banner_len as usize)
    };

    // Process safely
    let result = analyze_banner_safe(banner_slice, port);

    // Return allocated C struct
    Box::into_raw(Box::new(result))
}

fn analyze_banner_safe(banner: &[u8], port: u16) -> service_fingerprint_c_t {
    // Safe processing...
}
```

---

## Performance & Scalability

### Concurrency Model

**Single-threaded event loop per node**:
- No mutex contention
- No thread overhead
- Deterministic scheduling
- Perfect for epoll/io_uring

**Multi-node distributed model**:
- Each agent runs independent event loop
- Controller aggregates results asynchronously
- Network I/O between nodes is asynchronous

### Scalability Numbers (v3.2 Targets)

| Metric | Value |
|--------|-------|
| Max concurrent sockets (1 node) | 20,000 |
| Packet throughput (optimized) | 100,000+ PPS |
| Banner grab RTT (p50) | <15ms |
| Memory per connection | 6.5 KB |
| Total memory (20K conns) | ~130 MB |
| Max distributed agents | 256 |
| Max targets per scan | 2M IPs |
| Scheduling overhead | <1ms per 1000 tasks |

### Network Stack Optimization

```c
// TCP Socket Configuration
int enable_tcp_nodelay = 1;
setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &enable_tcp_nodelay, sizeof(int));

// Receive buffer tuning (for quick banner grabs)
int rcvbuf = 65536;
setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(int));

// Connection timeout via select/epoll timeout
struct timeval tv = {.tv_sec = 5, .tv_usec = 0};

// SO_REUSEADDR for rapid reconnection (scanner use case)
int reuse = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
```

---

## Distributed Architecture

### Controller-Agent Communication Protocol

**Heartbeat (every 30 seconds):**
```json
{
  "jsonrpc": "2.0",
  "method": "agent.heartbeat",
  "params": {
    "agent_id": 1,
    "timestamp": 1741187400000,
    "status": "healthy",
    "tasks_pending": 50,
    "tasks_completed": 1200,
    "metrics_snapshot": {
      "active_connections": 128,
      "throughput_pps": 1250
    }
  },
  "id": 1000001
}
```

**Task Batching (1000-5000 tasks per batch):**
```json
{
  "jsonrpc": "2.0",
  "method": "agent.execute_task_batch",
  "params": {
    "batch_id": "batch_2026_03_05_001",
    "scan_id": "scan_2026_03_05_001",
    "tasks": [
      {
        "task_id": "task_001",
        "target": "192.168.1.0/24",
        "ports": [22, 80, 443],
        "scan_config": {...}
      }
    ]
  }
}
```

### Result Aggregation

Controller collects results from all agents and:
1. Deduplicates findings (same host scanned by multiple agents)
2. Merges metrics (sum throughput, aggregate latency)
3. Generates unified report
4. Exports in requested format

---

## Plugin System Design

### Plugin Directory Structure

```
/opt/blackmap-plugins/
  ├─ manifests/
  │   ├─ custom_ssh_probe.json
  │   ├─ web_scanner.json
  │   └─ custom_rce_detector.json
  ├─ library/
  │   ├─ libcustom_ssh_probe.so
  │   ├─ libweb_scanner.so
  │   └─ libcustom_rce_detector.so
  └─ databases/
      ├─ fingerprints_extended.toml
      ├─ cve_signatures.json
      └─ os_patterns.json
```

### Plugin Lifecycle

```
LOAD:
  1. Read manifest (JSON)
  2. dlopen() .so
  3. Resolve symbols
  4. Call plugin_init()
  5. Register with plugin_manager
  
EXECUTE:
  1. Call plugin_fn() with input
  2. Receive results
  3. Log execution
  
UNLOAD:
  1. Call plugin_cleanup()
  2. dlclose()
  3. Free resources
```

---

## Development Roadmap

### Phase 1: Foundation (Weeks 1-4)

- [x] Architecture design documentation
- [ ] Advanced stealth system (C)
  - [ ] Fragmentation module
  - [ ] Decoy host simulation
  - [ ] Rate-limiting detection
  - [ ] Adaptive jitter
- [ ] CLI extensions for stealth options

### Phase 2: Intelligence (Weeks 5-8)

- [ ] Adaptive Scanning Engine (C)
  - [ ] Network feedback collection
  - [ ] RTT-based adaptation
  - [ ] Concurrency adjustment
- [ ] Rust fingerprinting enhancements
  - [ ] 30+ service database
  - [ ] TOML/JSON fingerprint loading
  - [ ] CPE/CVE mapping

### Phase 3: Metrics & Observability (Weeks 9-12)

- [ ] Enhanced metrics engine
  - [ ] Time-series collection
  - [ ] Prometheus export
  - [ ] SQLite persistence
- [ ] Metrics visualization CLI
- [ ] Dashboard templates

### Phase 4: Distributed & Plugins (Weeks 13-16)

- [ ] Distributed controller-agent system
- [ ] JSON-RPC communication
- [ ] Plugin system framework
  - [ ] Plugin loader (dlopen)
  - [ ] Manifest parser
  - [ ] Example plugins

### Phase 5: Performance & Polish (Weeks 17-20)

- [ ] io_uring backend implementation
- [ ] Memory optimization & tuning
- [ ] Performance benchmarking
- [ ] Documentation completion

### Phase 6: Testing & Release (Weeks 21-24)

- [ ] Comprehensive test suite
- [ ] Security audit
- [ ] Performance validation
- [ ] Official v3.2.0 release

---

## CLI Interface (v3.2 Complete)

```bash
# Basic scanning
blackmap-v32 -p 22,80,443 192.168.1.0/24

# Advanced stealth
blackmap-v32 --stealth-level 3 --fragment --decoy 8.8.8.8,1.1.1.1 \
             --jitter-coefficient 0.8 --detect-rate-limit \
             192.168.1.0/24

# Adaptive scanning with detailed output
blackmap-v32 --enable-adaptive --export-metrics json \
             --metrics-format prometheus 192.168.1.0/24

# Distributed scanning
blackmap-v32 --distributed --agents "agent1:9000,agent2:9000,agent3:9000" \
             --controller-port 8000 192.168.1.0/24

# With custom stealth profile
blackmap-v32 --stealth-profile my_stealth.json --plugin-dir /opt/plugins \
             192.168.1.0/24

# Full configuration from file
blackmap-v32 --config scan_config.yaml

# Metrics export options
blackmap-v32 --export-metrics json --export-db scan_results.db \
             --prometheus-endpoint /metrics 192.168.1.0/24
```

---

## Configuration File (YAML)

```yaml
# blackmap_config.yaml
target:
  ips:
    - 192.168.1.0/24
    - 10.0.0.1
  ports: [22, 80, 443, 3306, 5432]

scanning:
  io_engine: io_uring          # select, epoll, io_uring
  scan_type: connect            # connect, syn (root)
  max_concurrency: 256
  timeout_ms: 5000
  retries: 2

stealth:
  preset: balanced
  enable_fragmentation: true
  fragment_size: 64
  decoys:
    - 8.8.8.8
    - 1.1.1.1
  jitter_coefficient: 0.7
  detect_rate_limit: true
  adaptive_timing: true

detection:
  enable_service_detection: true
  enable_version_detection: true
  enable_os_detection: false
  fingerprint_db: /opt/blackmap/data/fingerprints.toml

adaptive:
  enabled: true
  increase_on_low_variance: true
  decrease_on_timeout: true

plugins:
  directory: /opt/blackmap-plugins
  auto_load: true
  custom_probes:
    - name: custom_ssh
      path: /opt/plugins/ssh_probe.so

metrics:
  collect_detailed: true
  export_formats:
    - json
    - prometheus
    - sqlite
  prometheus_endpoint: /metrics
  sqlite_file: metrics.db
  time_series_retention_hours: 24

output:
  format: json
  file: results.json
  pretty_print: true
```

---

## Summary: Key Improvements Over v3.0

| Feature | v3.0 | v3.2 |
|---------|------|------|
| **Stealth Profiles** | 5 | 12 |
| **Adaptive Behavior** | Manual RTT | Auto-adjust CC/TO/Retries |
| **Max Concurrency** | 256 | 20,000+ |
| **Services** | 10 | 30+ |
| **Metrics Export** | JSON, CSV | JSON, Prometheus, SQLite |
| **Distributed Mode** | No | Yes (Controller/Agent) |
| **Plugin System** | No | Yes |
| **Fingerprint DB** | Embedded | External + plugins |
| **io_uring** | No | Yes (optional) |
| **Memory per conn** | 4KB | 6.5KB (buffered) |

---

## Conclusion

BlackMap v3.2 transforms the modular v3.0 architecture into a **production-grade reconnaissance platform** suitable for:

- Large-scale network assessments (millions of IPs)
- Sensitive security testing (dynamic stealth adaptation)
- Enterprise reconnaissance (distributed scanning)
- Custom detection workflows (plugin system)
- Operational metrics tracking (time-series, Prometheus)

The design maintains **C and Rust as the sole implementation languages**, ensuring:
- **Raw performance** for high-throughput scanning
- **Memory efficiency** for large datasets
- **Type safety** via Rust FFI boundaries
- **No external dependencies** on scripting runtimes

The modular architecture enables teams to:
- Extend functionality via plugins
- Integrate custom fingerprint databases
- Deploy distributed agents for multi-region campaigns
- Track detailed metrics for optimization
- Maintain full control over behavioral profiles

