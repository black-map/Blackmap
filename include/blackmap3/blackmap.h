#ifndef BLACKMAP3_H
#define BLACKMAP3_H

#include <stdint.h>
#include "blackmap3/version.h"
#include "blackmap3/network.h"
#include "blackmap3/scheduler.h"
#include "blackmap3/stealth.h"
#include "blackmap3/analysis.h"
#include "blackmap3/metrics.h"

/* ====================================================================
   BLACKMAP 3.0 - MAIN ORCHESTRATOR
   
   Architecture:
   
   Target IP/Ports
        ↓
   Scheduler (task queue)
        ↓
   Network Engine (epoll I/O)
        ↓
   Connection State Machine
        ↓
   Stealth System (pacing, delays)
        ↓
   Rust Analysis (banner parsing)
        ↓
   Metrics Collection
        ↓
   Output Formatting
   ==================================================================== */

typedef struct {
    // Core components
    network_engine_t *net_engine;
    scheduler_t *scheduler;
    stealth_ctx_t *stealth;
    metrics_engine_t *metrics;
    
    // Configuration
    scan_plan_t *plan;
    stealth_level_t stealth_level;
    
    // State
    int scan_running;
    int scan_completed;
    
    // Results
    host_result_t *results;
    uint32_t result_count;
} blackmap_t;

/* ====================================================================
   MAIN BLACKMAP API
   ==================================================================== */

// Create BlackMap scanner instance
blackmap_t* blackmap_create(void);

// Configure scanner
int blackmap_configure(
    blackmap_t *bm,
    const char **target_ips,
    uint32_t num_targets,
    const uint16_t *ports,
    uint32_t num_ports,
    stealth_level_t stealth,
    uint32_t max_concurrency_global,
    uint32_t max_concurrency_per_host
);

// Run scan (blocking)
int blackmap_scan(blackmap_t *bm);

// Get results after scan
const host_result_t* blackmap_get_results(
    blackmap_t *bm,
    uint32_t *out_count
);

// Free scanner instance
void blackmap_free(blackmap_t *bm);

/* ====================================================================
   QUERY RESULTS AFTER SCAN
   ==================================================================== */

// Find host result by IP
const host_result_t* blackmap_find_host(
    blackmap_t *bm,
    const char *ip
);

// Get all open ports for a host
const uint16_t* blackmap_get_open_ports(
    blackmap_t *bm,
    const char *ip,
    uint32_t *out_count
);

// Get service info for specific port
const service_info_t* blackmap_get_service_info(
    blackmap_t *bm,
    const char *ip,
    uint16_t port
);

/* ====================================================================
   METRICS & REPORTING
   ==================================================================== */

// Get final metrics
metrics_snapshot_t blackmap_get_metrics(blackmap_t *bm);

// Print human-readable results
void blackmap_print_results(blackmap_t *bm);

// Export to JSON
char* blackmap_export_json(blackmap_t *bm);

// Export to CSV
char* blackmap_export_csv(blackmap_t *bm);

#endif // BLACKMAP3_H
