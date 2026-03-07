#ifndef BLACKMAP3_METRICS_H
#define BLACKMAP3_METRICS_H

#include <stdint.h>
#include <time.h>

/* ====================================================================
   METRICS SYSTEM 3.0
   
   Real-time measurement of:
   - Per-port timing
   - Per-host timing
   - RTT statistics
   - Effective concurrency
   - Timeout rates
   - Throughput
   ==================================================================== */

typedef struct {
    // Timing information
    struct timespec scan_start;
    struct timespec scan_end;
    
    // Connection statistics
    uint64_t total_connections_attempted;
    uint64_t total_connections_open;
    uint64_t total_connections_closed;
    uint64_t total_timeouts;
    uint64_t total_errors;
    
    // RTT tracking
    uint64_t total_rtt_us;     // Sum of all RTTs
    uint32_t rtt_measurements; // Count for average calc
    uint64_t min_rtt_us;
    uint64_t max_rtt_us;
    
    // Data transferred
    uint64_t bytes_sent;
    uint64_t bytes_received;
    
    // Throughput
    uint32_t ports_scanned;
    uint32_t hosts_scanned;
    
    // Concurrency
    uint32_t peak_concurrency;
    uint32_t avg_concurrency;
    
    // Service detection
    uint32_t services_detected;
    uint32_t services_failed;
    
    // Flags
    uint32_t has_ipv4 : 1;
    uint32_t has_ipv6 : 1;
    uint32_t has_udp : 1;
} metrics_snapshot_t;

/* ====================================================================
   METRICS ENGINE
   ==================================================================== */

typedef struct {
    metrics_snapshot_t snapshot;
    uint32_t enabled;  // Metrics collection enabled?
} metrics_engine_t;

// Create metrics engine
metrics_engine_t* metrics_create(void);

// Free metrics engine
void metrics_free(metrics_engine_t *m);

// Event recording
void metrics_record_connection_attempt(metrics_engine_t *m);
void metrics_record_connection_open(metrics_engine_t *m);
void metrics_record_connection_timeout(metrics_engine_t *m);
void metrics_record_connection_error(metrics_engine_t *m);
void metrics_record_rtt(metrics_engine_t *m, uint64_t rtt_us);
void metrics_record_bytes_sent(metrics_engine_t *m, uint64_t bytes);
void metrics_record_bytes_received(metrics_engine_t *m, uint64_t bytes);
void metrics_record_service_detected(metrics_engine_t *m);
void metrics_record_service_failed(metrics_engine_t *m);
void metrics_record_peak_concurrency(metrics_engine_t *m, uint32_t concurrent);
void metrics_record_ports_scanned(metrics_engine_t *m, uint32_t count);
void metrics_record_hosts_scanned(metrics_engine_t *m, uint32_t count);

// Get current snapshot
metrics_snapshot_t metrics_get_snapshot(metrics_engine_t *m);

// Reset metrics
void metrics_reset(metrics_engine_t *m);

/* ====================================================================
   METRICS REPORTING
   ==================================================================== */

// Print metrics to stdout as table
void metrics_print_table(const metrics_snapshot_t *snap);

// Serialize metrics to JSON
char* metrics_to_json(const metrics_snapshot_t *snap);

// Calculate derived metrics
typedef struct {
    float elapsed_seconds;
    float ports_per_second;
    float avg_rtt_ms;
    float success_rate_percent;
    float timeout_rate_percent;
} metrics_derived_t;

metrics_derived_t metrics_derive(const metrics_snapshot_t *snap);

#endif // BLACKMAP3_METRICS_H
