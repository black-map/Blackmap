# BlackMap v3.2 - Module Implementation Specifications

**Version**: 3.2.0  
**Document Type**: Technical Implementation Guide  
**Audience**: C/Rust developers implementing v3.2 modules

---

## Table of Contents

1. [Stealth Module (C)](#stealth-module-c)
2. [Adaptive Engine (C)](#adaptive-engine-c)
3. [Rust Analysis Engine](#rust-analysis-engine)
4. [Metrics Engine (C)](#metrics-engine-c)
5. [Distributed Components](#distributed-components)
6. [Plugin System (C)](#plugin-system-c)
7. [Integration Points](#integration-points)

---

## Stealth Module (C)

### File Structure
```
src/core/stealth/
├── stealth_base.c          # Core steal profiles (v3.0 compatible)
├── stealth_fragmentation.c # NEW: Packet fragmentation
├── stealth_decoys.c        # NEW: Decoy host simulation
├── stealth_adaptive.c      # NEW: Adaptive timing jitter
├── stealth_ratelimit.c     # NEW: Rate-limit detection & backoff
└── stealth_custom.c        # NEW: Custom profile blending
```

### Header: `include/blackmap3/stealth.h` (Extended)

#### New Enumerations

```c
/* Fragmentation randomization strategies */
typedef enum {
    FRAG_SIZE_FIXED         = 0,   // Constant size (standard)
    FRAG_SIZE_RANDOM_20     = 1,   // Vary ±20%
    FRAG_SIZE_RANDOM_50     = 2    // Vary ±50% (very suspicious)
} fragmentation_strategy_t;

/* Decoy placement patterns */
typedef enum {
    DECOY_RANDOM            = 0,   // Intersperse randomly
    DECOY_ROUND_ROBIN       = 1,   // Cycle through decoys
    DECOY_CLUSTERED         = 2    // Group decoys, then real targets
} decoy_pattern_t;

/* Jitter distribution algorithms */
typedef enum {
    JITTER_UNIFORM          = 0,   // Equal probability (0-max)
    JITTER_EXPONENTIAL      = 1,   // Bias toward lower values
    JITTER_POISSON          = 2    // Point process (realistic traffic)
} jitter_distribution_t;

/* Rate-limit detection algorithms */
typedef enum {
    RATELIMIT_NONE          = 0,   // Disabled
    RATELIMIT_TIMEOUT_SPIKE = 1,   // Detect RTT increase
    RATELIMIT_ERROR_RATE    = 2,   // Detect error percentage
    RATELIMIT_COMBINED      = 3    // Both methods
} ratelimit_detection_t;
```

#### Fragmentation Configuration

```c
typedef struct {
    uint8_t enabled;
    uint16_t fragment_size;        // 20-1500 bytes
    fragmentation_strategy_t strategy;
    uint32_t inter_fragment_delay_us;
    uint8_t randomize_fragment_order;  // Send fragments out of order
    uint8_t randomize_port_per_fragment;  // Each fragment from different port
    
    // Statistics
    uint64_t fragments_sent;
    uint64_t original_packets_fragmented;
} fragmentation_config_t;

/* Fragment tracking in connection structure */
typedef struct {
    uint8_t *fragments[64];         // Up to 64 fragments per packet
    uint32_t fragment_lens[64];
    uint8_t fragment_order[64];     // Actual transmission order
    uint32_t fragment_count;
    uint64_t last_fragment_sent_us;
} fragment_tracker_t;
```

#### Decoy Configuration

```c
typedef struct {
    char **decoy_ips;
    uint32_t decoy_count;
    decoy_pattern_t pattern;
    uint8_t decoy_ratio;            // 0-100: % real targets
    uint32_t decoy_cycle_position;  // For round-robin
    
    // Decoy generation
    uint8_t generate_spoofed_macs;
    mac_address_t spoofed_mac;      // When enabled
    
    // Statistics
    uint64_t decoy_packets_sent;
    uint64_t real_packets_sent;
} decoy_config_t;

/* Decoy host context */
typedef struct {
    uint32_t decoy_index;
    uint64_t decoy_last_used_us;
    uint8_t is_decoy;               // Boolean flag
} decoy_context_t;
```

#### Adaptive Timing Configuration

```c
typedef struct {
    jitter_distribution_t distribution;
    float jitter_coefficient;       // 0.0-1.0
    uint32_t base_delay_us;
    
    // Adaptive state
    float current_multiplier;
    uint32_t adjustment_count;
    
    // RTT-aware adaptation
    uint64_t observed_min_rtt_us;
    uint64_t observed_avg_rtt_us;
    uint64_t rtt_history[256];      // Circular buffer
    uint32_t rtt_history_idx;
    
    // Timeout tracking
    uint32_t timeout_count;
    uint32_t consecutive_timeouts;
    uint64_t last_timeout_time_us;
} adaptive_timing_t;

/* Generate jitter using configured distribution */
uint32_t stealth_generate_jitter(
    stealth_config_v32_t *config,
    uint32_t base_delay_us
);

uint32_t stealth_jitter_uniform(uint32_t max_us);
uint32_t stealth_jitter_exponential(uint32_t max_us);
uint32_t stealth_jitter_poisson(uint32_t lambda_us);
```

#### Rate-Limit Detection Configuration

```c
typedef struct {
    ratelimit_detection_t enabled;
    uint32_t measurement_window_ms;
    float error_rate_threshold;     // 0.0-1.0 (e.g., 0.3 = 30%)
    float rtt_increase_threshold;   // 2.0 = 200% increase triggers backoff
    uint32_t samples_for_detection; // How many failures to detect
    
    // Backoff state
    float current_backoff_multiplier;
    uint32_t consecutive_backoff_activations;
    uint64_t last_detection_time_us;
    
    int is_rate_limited;            // Current state
} rate_limit_detection_t;

int stealth_detect_rate_limit(
    stealth_config_v32_t *config,
    uint64_t recent_rtt_us,
    uint8_t recent_success
);

uint32_t stealth_get_backoff_delay_ms(
    stealth_config_v32_t *config
);
```

### Implementation Details

#### Fragmentation Algorithm (Pseudo-C)

```c
int stealth_fragment_packet(
    stealth_config_v32_t *config,
    const uint8_t *packet,
    uint32_t packet_len,
    fragment_tracker_t *tracker
) {
    if (!config->fragmentation.enabled || packet_len < 100) {
        return 0;  // Don't fragment small packets
    }

    uint16_t frag_size = config->fragmentation.fragment_size;
    
    // Apply size randomization
    if (config->fragmentation.strategy == FRAG_SIZE_RANDOM_20) {
        frag_size += (rand() % (frag_size / 5)) - (frag_size / 10);
    } else if (config->fragmentation.strategy == FRAG_SIZE_RANDOM_50) {
        frag_size += (rand() % frag_size) - (frag_size / 2);
    }

    // Split packet into fragments
    uint32_t offset = 0;
    uint32_t frag_count = 0;
    
    while (offset < packet_len && frag_count < 64) {
        uint32_t chunk_len = MIN(frag_size, packet_len - offset);
        tracker->fragments[frag_count] = malloc(chunk_len);
        memcpy(tracker->fragments[frag_count], packet + offset, chunk_len);
        tracker->fragment_lens[frag_count] = chunk_len;
        
        // Original order
        tracker->fragment_order[frag_count] = frag_count;
        
        offset += chunk_len;
        frag_count++;
    }

    // Randomize transmission order (optional)
    if (config->fragmentation.enabled > 1) {
        fisher_yates_shuffle(tracker->fragment_order, frag_count);
    }

    tracker->fragment_count = frag_count;
    return 0;
}

int stealth_send_fragments(
    stealth_config_v32_t *config,
    int sock,
    fragment_tracker_t *tracker
) {
    for (uint32_t i = 0; i < tracker->fragment_count; i++) {
        uint32_t order_idx = tracker->fragment_order[i];
        
        send(sock,
             tracker->fragments[order_idx],
             tracker->fragment_lens[order_idx],
             0);
        
        // Inter-fragment delay
        usleep(config->fragmentation.inter_fragment_delay_us);
        
        config->fragmentation.fragments_sent++;
    }
    
    return 0;
}
```

#### Decoy Integration

```c
int stealth_select_target_with_decoy(
    stealth_config_v32_t *config,
    const char **target_ips,
    uint32_t target_count,
    const char **decoy_ips,
    uint32_t decoy_count,
    char *selected_ip
) {
    // Determine if we're sending to decoy or real target
    uint8_t use_decoy = (rand() % 100) >= config->decoys.decoy_ratio;
    
    if (use_decoy && decoy_count > 0) {
        // Select decoy based on pattern
        uint32_t idx;
        
        if (config->decoys.pattern == DECOY_ROUND_ROBIN) {
            idx = config->decoys.decoy_cycle_position % decoy_count;
            config->decoys.decoy_cycle_position++;
        } else {
            idx = rand() % decoy_count;
        }
        
        strcpy(selected_ip, decoy_ips[idx]);
        config->decoys.decoy_packets_sent++;
        return 1;  // Decoy flag
    } else {
        // Select real target
        uint32_t idx = rand() % target_count;
        strcpy(selected_ip, target_ips[idx]);
        config->decoys.real_packets_sent++;
        return 0;  // Real flag
    }
}
```

---

## Adaptive Engine (C)

### File Structure
```
src/core/adaptive/
├── adaptive_engine.c       # Core adaptive logic
├── adaptive_feedback.c     # Network feedback collection
├── adaptive_metrics.c      # Measurement tracking
└── adaptive_adjustment.c   # Parameter adjustment logic
```

### Header: `include/blackmap3/adaptive.h`

```c
#ifndef BLACKMAP3_ADAPTIVE_H
#define BLACKMAP3_ADAPTIVE_H

#include <stdint.h>
#include "network.h"

typedef enum {
    ADAPT_PHASE_INITIAL     = 0,
    ADAPT_PHASE_ANALYSIS    = 1,
    ADAPT_PHASE_ADJUSTMENT  = 2,
    ADAPT_PHASE_MONITORING  = 3
} adaptation_phase_t;

typedef struct {
    // Concurrency adaptation
    uint32_t base_global_concurrency;
    uint32_t current_global_concurrency;
    uint32_t min_global_concurrency;
    uint32_t max_global_concurrency;
    
    float rtt_variance_for_increase;    // 0.1 = 10% stddev signals increase
    float error_rate_for_decrease;      // 0.05 = 5% error triggers decrease
    float increase_multiplier;          // 1.2x increase
    float decrease_multiplier;          // 0.7x decrease
} concurrency_adaptation_t;

typedef struct {
    // Timeout adaptation
    uint32_t base_timeout_ms;
    uint32_t current_timeout_ms;
    uint32_t min_timeout_ms;
    uint32_t max_timeout_ms;
    
    float rtt_safety_margin;            // 1.5x observed RTT
} timeout_adaptation_t;

typedef struct {
    // Retry adaptation
    uint32_t base_retries;
    uint32_t current_retries;
    uint32_t max_retries;
    
    uint8_t enable_exponential_backoff;
    float backoff_base;                 // 2.0 for doubling
} retry_adaptation_t;

typedef struct {
    // Network measurements
    uint64_t *rtt_samples;              // Ring buffer of recent RTTs
    uint32_t rtt_sample_count;
    uint32_t rtt_sample_capacity;
    uint32_t rtt_sample_idx;
    
    // Statistics
    uint64_t rtt_min_us;
    uint64_t rtt_max_us;
    uint64_t rtt_avg_us;
    uint32_t rtt_stddev_us;
    
    // Loss tracking
    uint32_t total_attempted;
    uint32_t total_succeeded;
    uint32_t total_timed_out;
    uint32_t total_errors;
    float packet_loss_rate;
    
    // Timeouts
    uint32_t consecutive_timeouts;
    uint64_t last_success_time_us;
    uint64_t last_timeout_time_us;
} network_feedback_t;

typedef struct {
    adaptation_phase_t phase;
    uint64_t phase_start_time_us;
    uint32_t measurements_in_phase;
    
    concurrency_adaptation_t concurrency;
    timeout_adaptation_t timeout;
    retry_adaptation_t retry;
    network_feedback_t feedback;
    
    // Decision logging
    char last_decision[256];
    uint32_t decision_count;
    uint64_t last_adjustment_time_us;
    uint32_t adjustment_count;
} adaptive_engine_t;

/* Initialization */
adaptive_engine_t* adaptive_engine_init(
    uint32_t base_concurrency,
    uint32_t base_timeout_ms,
    uint32_t base_retries
);

/* Feedback collection */
void adaptive_engine_record_measurement(
    adaptive_engine_t *ae,
    uint64_t rtt_us,
    uint8_t success
);

void adaptive_engine_record_timeout(
    adaptive_engine_t *ae
);

/* Analysis & adjustment */
typedef struct {
    uint32_t recommended_concurrency;
    uint32_t recommended_timeout_ms;
    uint32_t recommended_retries;
    uint8_t should_enable_backoff;
    char reasoning[512];
} adaptation_recommendation_t;

adaptation_recommendation_t adaptive_engine_analyze(
    adaptive_engine_t *ae
);

int adaptive_engine_apply_recommendations(
    adaptive_engine_t *ae,
    const adaptation_recommendation_t *rec
);

/* State query */
void adaptive_engine_get_stats(
    adaptive_engine_t *ae,
    network_feedback_t *feedback
);

/* Cleanup */
void adaptive_engine_free(adaptive_engine_t *ae);

#endif
```

### Implementation Example

```c
adaptation_recommendation_t adaptive_engine_analyze(
    adaptive_engine_t *ae
) {
    adaptation_recommendation_t rec = {
        .recommended_concurrency = ae->concurrency.current_global_concurrency,
        .recommended_timeout_ms = ae->timeout.current_timeout_ms,
        .recommended_retries = ae->retry.current_retries,
        .should_enable_backoff = 0
    };

    network_feedback_t *fb = &ae->feedback;

    // Require minimum measurements before analysis
    if (fb->rtt_sample_count < 20) {
        strcpy(rec.reasoning, "Insufficient samples for analysis");
        return rec;
    }

    // Compute statistics
    adaptive_compute_rtt_stats(ae);

    // Rule 1: Detect network stability
    float rtt_cv = (float)fb->rtt_stddev_us / fb->rtt_avg_us;
    
    if (rtt_cv < 0.1) {
        // Network is very stable, increase concurrency
        rec.recommended_concurrency = MIN(
            ae->concurrency.current_global_concurrency * 1.2,
            ae->concurrency.max_global_concurrency
        );
        snprintf(rec.reasoning, sizeof(rec.reasoning),
                 "Network stable (CV=%.2f), increasing concurrency", rtt_cv);
    } else if (rtt_cv > 0.5) {
        // Network is unstable, decrease concurrency
        rec.recommended_concurrency = MAX(
            ae->concurrency.current_global_concurrency * 0.7,
            ae->concurrency.min_global_concurrency
        );
        snprintf(rec.reasoning, sizeof(rec.reasoning),
                 "Network unstable (CV=%.2f), decreasing concurrency", rtt_cv);
    }

    // Rule 2: Packet loss detection
    if (fb->packet_loss_rate > 0.05) {
        // Increase timeout and reduce concurrency
        rec.recommended_timeout_ms = MIN(
            ae->timeout.current_timeout_ms * 1.3,
            ae->timeout.max_timeout_ms
        );
        
        rec.recommended_concurrency = MAX(
            ae->concurrency.current_global_concurrency * 0.8,
            ae->concurrency.min_global_concurrency
        );
        
        snprintf(rec.reasoning, sizeof(rec.reasoning),
                 "Packet loss detected (%.1f%%), backing off",
                 fb->packet_loss_rate * 100.0);
    }

    // Rule 3: Timeout spike detection
    if (fb->consecutive_timeouts > 3) {
        rec.should_enable_backoff = 1;
        rec.recommended_timeout_ms = MIN(
            ae->timeout.current_timeout_ms * 1.5,
            ae->timeout.max_timeout_ms
        );
        
        snprintf(rec.reasoning, sizeof(rec.reasoning),
                 "Consecutive timeouts detected (%u), enabling backoff",
                 fb->consecutive_timeouts);
    }

    return rec;
}
```

---

## Rust Analysis Engine

### File Structure
```
rust/src/
├── lib.rs                  # Main FFI boundary
├── fingerprint/
│   ├── mod.rs
│   ├── http.rs            # HTTP/HTTPS detection
│   ├── ssh.rs             # SSH detection
│   ├── database.rs        # Database services
│   ├── mail.rs            # SMTP/Mail services
│   └── misc.rs            # Other services
├── database/
│   ├── mod.rs
│   ├── loader.rs          # TOML/JSON loading
│   └── service_db.rs      # Database management
├── plugins/
│   ├── mod.rs
│   └── loader.rs          # Plugin system
└── metrics.rs             # Performance tracking
```

### Service Detection Implementation

#### HTTP Detection Example

```rust
// rust/src/fingerprint/http.rs

use regex::Regex;
use lazy_static::lazy_static;

lazy_static! {
    static ref APACHE_VERSION: Regex = 
        Regex::new(r"Apache/(\d+\.\d+\.\d+[a-zA-Z0-9]*)").unwrap();
    
    static ref NGINX_VERSION: Regex = 
        Regex::new(r"nginx/(\d+\.\d+\.\d+[a-zA-Z0-9]*)").unwrap();
    
    static ref IIS_VERSION: Regex = 
        Regex::new(r"IIS/(\d+\.\d+)").unwrap();
    
    static ref HTTP_SERVER_HEADER: Regex = 
        Regex::new(r"Server:\s*([^\r\n]+)").unwrap();
    
    static ref PHP_DETECTION: Regex = 
        Regex::new(r"X-Powered-By:\s*PHP/([0-9.]+)").unwrap();
}

pub struct HttpDetector;

impl HttpDetector {
    pub fn detect(banner: &[u8], port: u16) -> Option<ServiceFingerprint> {
        let banner_str = String::from_utf8_lossy(banner);
        
        // Check if HTTP
        if !banner_str.contains("HTTP/") {
            return None;
        }

        // Extract server header
        let server_header = HTTP_SERVER_HEADER
            .captures(&banner_str)
            .and_then(|c| c.get(1))
            .map(|m| m.as_str());

        let (product, version, mut confidence) = match server_header {
            Some(srv) => {
                if APACHE_VERSION.is_match(srv) {
                    let ver = APACHE_VERSION
                        .captures(srv)
                        .and_then(|c| c.get(1))
                        .map(|m| m.as_str().to_string());
                    ("Apache HTTP Server".to_string(), ver, 95)
                } else if NGINX_VERSION.is_match(srv) {
                    let ver = NGINX_VERSION
                        .captures(srv)
                        .and_then(|c| c.get(1))
                        .map(|m| m.as_str().to_string());
                    ("nginx".to_string(), ver, 95)
                } else if IIS_VERSION.is_match(srv) {
                    let ver = IIS_VERSION
                        .captures(srv)
                        .and_then(|c| c.get(1))
                        .map(|m| m.as_str().to_string());
                    ("Microsoft IIS".to_string(), ver, 90)
                } else {
                    (srv.to_string(), None, 50)
                }
            }
            None => ("HTTP Server".to_string(), None, 60)
        };

        // Detect technology stack
        let mut metadata = std::collections::HashMap::new();
        
        if let Some(php_match) = PHP_DETECTION.captures(&banner_str) {
            if let Some(php_ver) = php_match.get(1) {
                metadata.insert("php_version".to_string(), php_ver.as_str().to_string());
                confidence += 10;
            }
        }

        Some(ServiceFingerprint {
            service: "HTTP".to_string(),
            product,
            version,
            confidence: (confidence as u8).min(100),
            detected_via: "banner_match".to_string(),
            metadata,
            cpe: generate_cpe("apache", product, version),
            os_info: None,
        })
    }
}

fn generate_cpe(vendor: &str, product: &str, version: Option<String>) -> Option<String> {
    match (&product, &version) {
        (_, Some(v)) => Some(format!("cpe:/a:{}:{}:{}", vendor, product, v)),
        _ => None
    }
}
```

#### SSH Detection

```rust
// rust/src/fingerprint/ssh.rs

pub struct SshDetector;

impl SshDetector {
    pub fn detect(banner: &[u8], port: u16) -> Option<ServiceFingerprint> {
        let banner_str = String::from_utf8_lossy(banner);
        
        // SSH banner format: SSH-2.0-OpenSSH_7.4p1 ...
        if !banner_str.starts_with("SSH-") {
            return None;
        }

        let mut metadata = std::collections::HashMap::new();
        let mut confidence = 99u8;
        let mut product = "Unknown SSH".to_string();
        let mut version = None;

        // Parse SSH banner
        let parts: Vec<&str> = banner_str.split('-').collect();
        if parts.len() >= 3 {
            let impl_str = parts[2].trim();
            
            if impl_str.starts_with("OpenSSH_") {
                product = "OpenSSH".to_string();
                let ver_part = impl_str.strip_prefix("OpenSSH_").unwrap();
                version = Some(ver_part.to_string());
                
                // Check for known vulnerability patterns
                if ver_part.starts_with("7.4p1") {
                    metadata.insert("known_issues".to_string(), 
                                  "CVE-2018-15473".to_string());
                }
            } else if impl_str.contains("libssh") {
                product = "libssh".to_string();
                confidence = 85;
            }
            
            metadata.insert("raw_implementation".to_string(), impl_str.to_string());
        }

        Some(ServiceFingerprint {
            service: "SSH".to_string(),
            product,
            version,
            confidence,
            detected_via: "banner_match".to_string(),
            metadata,
            cpe: None,
            os_info: None,
        })
    }
}
```

### Fingerprint Database Loading

```rust
// rust/src/database/loader.rs

use toml::Value;
use std::fs;
use std::collections::HashMap;

pub struct FingerprintDatabase {
    services: HashMap<String, ServiceDefinition>,
}

pub struct ServiceDefinition {
    name: String,
    port: u16,
    patterns: Vec<Pattern>,
    heuristics: Vec<Heuristic>,
}

pub struct Pattern {
    pattern_type: String,  // "banner", "header", "status"
    regex: Regex,
    extract_version: bool,
    confidence_boost: u8,
}

pub struct Heuristic {
    rule: String,
    confidence_boost: u8,
}

impl FingerprintDatabase {
    pub fn load_toml(path: &str) -> Result<Self, String> {
        let contents = fs::read_to_string(path)
            .map_err(|e| format!("Failed to read file: {}", e))?;
        
        let value: Value = toml::from_str(&contents)
            .map_err(|e| format!("TOML parse error: {}", e))?;

        let mut db = FingerprintDatabase {
            services: HashMap::new(),
        };

        // Parse services
        if let Some(services_table) = value.get("services").and_then(|v| v.as_table()) {
            for (service_name, service_data) in services_table {
                if let Ok(defs) = Self::parse_service_definitions(
                    service_name,
                    service_data
                ) {
                    for def in defs {
                        db.services.insert(
                            format!("{}:{}", service_name, def.port),
                            def
                        );
                    }
                }
            }
        }

        Ok(db)
    }

    fn parse_service_definitions(
        name: &str,
        data: &Value
    ) -> Result<Vec<ServiceDefinition>, String> {
        let mut defs = Vec::new();

        // Handle array of service definitions
        if let Some(arr) = data.as_array() {
            for item in arr {
                defs.push(Self::parse_single_definition(name, item)?);
            }
        } else {
            defs.push(Self::parse_single_definition(name, data)?);
        }

        Ok(defs)
    }

    pub fn match_banner(
        &self,
        banner: &[u8],
        port: u16
    ) -> Vec<ServiceFingerprint> {
        let mut results = Vec::new();

        // Try to match against all known services for this port
        for (_, service_def) in self.services.iter() {
            if service_def.port == port {
                for pattern in &service_def.patterns {
                    if let Ok(banner_str) = std::str::from_utf8(banner) {
                        if pattern.regex.is_match(banner_str) {
                            // Match found, build result
                            let fp = ServiceFingerprint {
                                service: service_def.name.clone(),
                                product: service_def.name.clone(),
                                version: if pattern.extract_version {
                                    pattern.regex.captures(banner_str)
                                        .and_then(|c| c.get(1))
                                        .map(|m| m.as_str().to_string())
                                } else {
                                    None
                                },
                                confidence: (50 + pattern.confidence_boost).min(100),
                                detected_via: "database_match".to_string(),
                                metadata: Default::default(),
                                cpe: None,
                                os_info: None,
                            };
                            
                            results.push(fp);
                        }
                    }
                }
            }
        }

        results
    }
}
```

---

## Metrics Engine (C)

### File Structure
```
src/metrics/
├── metrics_engine.c        # Core metrics collection
├── metrics_time_series.c   # Time-series sampling
├── metrics_export.c        # Export formats (JSON, SQLite, Prometheus)
└── metrics_query.c         # Metrics querying API
```

### Header: `include/blackmap3/metrics.h` (Extended)

```c
#ifndef BLACKMAP3_METRICS_H
#define BLACKMAP3_METRICS_H

#include <stdint.h>
#include <time.h>

/* ====== Per-Connection Metrics ====== */

typedef struct {
    uint32_t connection_id;
    char target_ip[46];
    uint16_t target_port;
    
    uint64_t connect_start_us;
    uint64_t connect_end_us;
    uint64_t rtt_us;
    
    uint8_t final_state;           // OPEN, CLOSED, FILTERED, TIMEOUT
    uint32_t bytes_sent;
    uint32_t bytes_received;
    uint32_t retry_count;
    
    char banner[4096];
    uint32_t banner_len;
} connection_metric_t;

/* ====== Port-Level Aggregates ====== */

typedef struct {
    uint16_t port;
    
    // Latency distribution
    uint64_t *rtt_samples;         // Ring buffer for percentiles
    uint32_t sample_count;
    uint64_t rtt_min_us;
    uint64_t rtt_p50_us, rtt_p90_us, rtt_p99_us;
    uint64_t rtt_max_us;
    
    // State counters
    uint32_t open_count;
    uint32_t closed_count;
    uint32_t filtered_count;
    uint32_t timeout_count;
    
    // Service detection
    service_fingerprint_c_t main_service;
    float confidence_avg;
} port_metric_t;

/* ====== Service-Level Aggregates ====== */

typedef struct {
    char service_name[128];
    char product[256];
    uint32_t instances_detected;
    
    // Version distribution
    char **detected_versions;
    uint32_t **version_counts;
    uint32_t version_count;
    
    // Vulnerability tracking
    uint32_t cve_list_count;
    char **cves;
    float avg_cvss_score;
} service_metric_t;

/* ====== Time-Series Sample ====== */

typedef struct {
    uint64_t sample_start_us;
    uint64_t sample_end_us;
    
    uint32_t active_connections;
    uint32_t connections_completed;
    float throughput_pps;
    float avg_rtt_ms;
    float packet_loss_percent;
    uint32_t timeout_events;
} time_series_sample_t;

typedef struct {
    time_series_sample_t *samples;      // Circular buffer
    uint32_t sample_count;
    uint32_t max_samples;               // Typically 3600
    uint32_t current_idx;
    uint32_t sample_interval_ms;
} time_series_metrics_t;

/* ====== Scan Summary ====== */

typedef struct {
    uint64_t scan_start_us;
    uint64_t scan_end_us;
    
    uint32_t targets_scanned;
    uint32_t total_ports_scanned;
    uint32_t total_connections_attempted;
    uint32_t total_connections_open;
    
    float throughput_pps;
    float avg_connection_duration_us;
    float peak_concurrent;
    
    uint32_t total_timeouts;
    uint32_t total_retries;
    float success_rate;
    
    uint32_t unique_services;
    float avg_detection_confidence;
} scan_summary_t;

/* ====== Metrics Engine ====== */

typedef struct {
    // Connection-level
    connection_metric_t *connections;
    uint32_t connection_count;
    uint32_t max_connections;
    
    // Port-level
    port_metric_t *port_metrics;
    uint32_t port_count;
    
    // Service-level
    service_metric_t *service_metrics;
    uint32_t service_count;
    
    // Time-series
    time_series_metrics_t time_series;
    
    // Scan summary
    scan_summary_t summary;
    
    // Sampling
    uint32_t sample_interval_ms;
    uint64_t last_sample_time_us;
} metrics_engine_t;

/* ====== API ====== */

metrics_engine_t* metrics_engine_init(uint32_t sample_interval_ms);

void metrics_record_connection(
    metrics_engine_t *me,
    const connection_metric_t *metric
);

void metrics_record_port_result(
    metrics_engine_t *me,
    uint16_t port,
    uint32_t state,
    uint64_t rtt_us
);

void metrics_record_service_detection(
    metrics_engine_t *me,
    const char *service,
    const char *version
);

void metrics_sample_time_series(
    metrics_engine_t *me,
    uint32_t active_conns,
    uint32_t completed,
    float throughput_pps
);

void metrics_finalize_scan(metrics_engine_t *me);

/* Export */
char* metrics_export_json(metrics_engine_t *me);
int metrics_export_sqlite(metrics_engine_t *me, const char *db_path);
char* metrics_export_prometheus(metrics_engine_t *me);

/* Querying */
port_metric_t* metrics_get_port(metrics_engine_t *me, uint16_t port);
service_metric_t* metrics_get_service(metrics_engine_t *me, const char *name);

void metrics_engine_free(metrics_engine_t *me);

#endif
```

---

## Distributed Components

### Controller Implementation (`src/distributed/controller.c`)

```c
#include "distributed.h"
#include <stdio.h>
#include <pthread.h>

distributed_controller_t* distributed_controller_init(
    const char **agent_endpoints,
    uint32_t agent_count
) {
    distributed_controller_t *ctrl = malloc(sizeof(*ctrl));
    
    ctrl->agent_endpoints = malloc(agent_count * sizeof(char*));
    for (uint32_t i = 0; i < agent_count; i++) {
        ctrl->agent_endpoints[i] = strdup(agent_endpoints[i]);
    }
    
    ctrl->agent_count = agent_count;
    ctrl->max_tasks_per_agent = 1000;
    
    ctrl->task_queue = task_queue_create(100000);
    ctrl->results = malloc(sizeof(completed_result_t) * 100000);
    ctrl->result_count = 0;
    
    ctrl->agent_statuses = malloc(agent_count * sizeof(agent_status_t));
    for (uint32_t i = 0; i < agent_count; i++) {
        ctrl->agent_statuses[i].agent_id = i;
        ctrl->agent_statuses[i].tasks_assigned = 0;
        ctrl->agent_statuses[i].is_healthy = 1;
    }
    
    return ctrl;
}

int distributed_controller_distribute_tasks(
    distributed_controller_t *ctrl,
    const task_t *tasks,
    uint32_t task_count
) {
    // Distribute tasks evenly across agents
    uint32_t tasks_per_agent = task_count / ctrl->agent_count;
    uint32_t remainder = task_count % ctrl->agent_count;
    
    uint32_t task_idx = 0;
    
    for (uint32_t agent_id = 0; agent_id < ctrl->agent_count; agent_id++) {
        uint32_t agent_task_count = tasks_per_agent +
            (agent_id < remainder ? 1 : 0);
        
        // Build task batch JSON
        char batch_json[1024 * 1024];
        distributed_build_task_batch_json(
            batch_json,
            sizeof(batch_json),
            tasks + task_idx,
            agent_task_count,
            agent_id
        );
        
        // Send to agent via JSON-RPC
        distributed_send_task_batch(
            ctrl->agent_endpoints[agent_id],
            batch_json
        );
        
        ctrl->agent_statuses[agent_id].tasks_assigned += agent_task_count;
        
        task_idx += agent_task_count;
    }
    
    return 0;
}
```

---

## Plugin System (C)

### Plugin Loader (`src/plugins/plugin_loader.c`)

```c
#include "plugin.h"
#include <dlfcn.h>
#include <stdio.h>

plugin_manager_t* plugin_manager_init(void) {
    plugin_manager_t *pm = malloc(sizeof(*pm));
    pm->plugins = malloc(32 * sizeof(loaded_plugin_t));
    pm->plugin_count = 0;
    return pm;
}

int plugin_manager_load(
    plugin_manager_t *pm,
    const char *plugin_path
) {
    if (pm->plugin_count >= 32) {
        fprintf(stderr, "Maximum plugins loaded\n");
        return -1;
    }

    // Load manifest first
    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s.json", plugin_path);
    
    // Parse JSON manifest (use simple parser or jsmn)
    plugin_metadata_t metadata;
    if (parse_plugin_manifest(manifest_path, &metadata) != 0) {
        return -1;
    }

    // dlopen the plugin library
    void *handle = dlopen(plugin_path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Failed to load plugin: %s\n", dlerror());
        return -1;
    }

    // Load entry point function
    plugin_init_fn init_fn = dlsym(handle, metadata.entry_point);
    if (!init_fn) {
        fprintf(stderr, "Failed to find entry point: %s\n", metadata.entry_point);
        dlclose(handle);
        return -1;
    }

    // Initialize plugin
    if (init_fn() != 0) {
        fprintf(stderr, "Plugin initialization failed\n");
        dlclose(handle);
        return -1;
    }

    // Register plugin
    loaded_plugin_t *loaded = &pm->plugins[pm->plugin_count];
    loaded->handle = handle;
    loaded->plugin_type = metadata.type;
    strcpy(loaded->plugin_path, plugin_path);
    
    pm->plugin_count++;
    
    printf("Loaded plugin: %s (type=%d)\n", metadata.name, metadata.type);
    
    return 0;
}

void plugin_manager_free(plugin_manager_t *pm) {
    for (uint32_t i = 0; i < pm->plugin_count; i++) {
        dlclose(pm->plugins[i].handle);
    }
    free(pm->plugins);
    free(pm);
}
```

---

## Integration Points

### Main Blackmap Flow (Updated for v3.2)

```c
// File: src/core/blackmap.c (updated)

int blackmap_scan_v32(blackmap_t *bm) {
    // 1. Initialize all v3.2 components
    stealth_config_v32_t *stealth = stealth_config_v32_init(bm->config.stealth);
    adaptive_engine_t *adaptive = adaptive_engine_init(256, 5000, 2);
    metrics_engine_t *metrics = metrics_engine_init(100);
    plugin_manager_t *plugins = plugin_manager_init();
    
    // Load plugins
    for (uint32_t i = 0; i < bm->config.plugin_count; i++) {
        plugin_manager_load(plugins, bm->config.plugin_paths[i]);
    }
    
    // 2. Generate scan plan
    task_t *tasks = blackmap_generate_plan(bm->config.target_ips,
                                          bm->config.target_count,
                                          bm->config.ports,
                                          bm->config.port_count);
    
    // 3. Load adaptive engine baseline
    uint32_t measurement_phase = 50;  // First 50 measurements
    
    // 4. Main event loop (epoll or io_uring)
    while (task_queue_has_pending(bm->scheduler)) {
        // 4a. Get next task
        task_t *task = scheduler_next_task(bm->scheduler);
        if (!task) break;
        
        // 4b. Apply stealth delays
        uint32_t delay_us = stealth_get_pre_connect_delay_us(stealth);
        usleep(delay_us);
        
        // 4c. Execute connection
        int sock = network_queue_connection(bm->net_engine, task);
        
        // 4d. Process network events
        connection_t *conn = network_process_batch(bm->net_engine, 5000);
        
        // 4e. Record metrics
        metrics_record_connection(metrics, /* ... */);
        adaptive_engine_record_measurement(adaptive, conn->rtt_us,
                                          conn->state == OPEN);
        
        // 4f. Adaptive analysis (every 100 tasks)
        if (adaptive->feedback.rtt_sample_count % 100 == 0) {
            adaptation_recommendation_t rec = adaptive_engine_analyze(adaptive);
            adaptive_engine_apply_recommendations(adaptive, &rec);
            
            // Update stealth timing if needed
            stealth->max_global_concurrency = rec.recommended_concurrency;
            stealth->timeout_ms = rec.recommended_timeout_ms;
        }
        
        // 4g. Banner analysis (Rust FFI)
        service_fingerprint_c_t *fp = analysis_fingerprint_banner(
            (uint8_t*)conn->banner,
            conn->banner_len,
            task->port
        );
        
        // 4h. Plugin hook
        if (bm->config.enable_plugins) {
            plugin_manager_execute_probes(plugins, sock, 
                                         (uint8_t*)conn->banner,
                                         conn->banner_len, fp);
        }
        
        // 4i. Record service detection
        metrics_record_service_detection(metrics, fp->service, fp->version);
        
        // 4j. Time-series sampling
        metrics_sample_time_series(metrics,
                                 network_get_active_connections(bm->net_engine),
                                 bm->scheduler->completed_count,
                                 network_get_throughput_pps(bm->net_engine));
        
        // Clean up
        analysis_free_fingerprint(fp);
        scheduler_mark_complete(bm->scheduler, task->id);
    }
    
    // 5. Finalize and export
    metrics_finalize_scan(metrics);
    
    char *json_results = metrics_export_json(metrics);
    printf("%s\n", json_results);
    
    if (bm->config.export_metrics_sqlite) {
        metrics_export_sqlite(metrics, bm->config.sqlite_path);
    }
    
    // 6. Cleanup
    stealth_config_free(stealth);
    adaptive_engine_free(adaptive);
    metrics_engine_free(metrics);
    plugin_manager_free(plugins);
    free(json_results);
    
    return 0;
}
```

---

## Summary: Implementation Priorities

**Phase 1 (Weeks 1-2):**
- Stealth module (fragmentation, decoys, adaptive timing)
- CLI updates for new stealth options
- Testing harness

**Phase 2 (Weeks 3-4):**
- Adaptive Engine (feedback collection, analysis, adjustments)
- Integration with stealth system
- Metrics tracking

**Phase 3 (Weeks 5-6):**
- Rust fingerprinting enhancements (30+ services, TOML loading)
- FFI boundary updates
- Service detection testing

**Phase 4 (Weeks 7-8):**
- metrics export (JSON, SQLite, Prometheus)
- Plugin system framework
- Distributed architecture (basic)

**Phase 5 (Weeks 9-10):**
- io_uring backend
- Performance tuning
- Comprehensive documentation

