/* ====================================================================
   BLACKMAP 3.0 - METRICS SYSTEM IMPLEMENTATION
   
   Real-time measurement and reporting
   ===================================================================== */

#include "blackmap3/metrics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

metrics_engine_t* metrics_create(void) {
    metrics_engine_t *m = (metrics_engine_t*)malloc(sizeof(*m));
    if (!m) return NULL;
    
    memset(m, 0, sizeof(*m));
    m->enabled = 1;
    
    // Initialize min/max
    m->snapshot.min_rtt_us = UINT64_MAX;
    m->snapshot.max_rtt_us = 0;
    
    // Record start time
    clock_gettime(CLOCK_MONOTONIC, &m->snapshot.scan_start);
    
    return m;
}

void metrics_free(metrics_engine_t *m) {
    if (m) free(m);
}

void metrics_record_connection_attempt(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.total_connections_attempted++;
}

void metrics_record_connection_open(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.total_connections_open++;
}

void metrics_record_connection_timeout(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.total_timeouts++;
}

void metrics_record_connection_error(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.total_errors++;
}

void metrics_record_rtt(metrics_engine_t *m, uint64_t rtt_us) {
    if (!m || !m->enabled) return;
    
    m->snapshot.total_rtt_us += rtt_us;
    m->snapshot.rtt_measurements++;
    
    if (rtt_us < m->snapshot.min_rtt_us) {
        m->snapshot.min_rtt_us = rtt_us;
    }
    if (rtt_us > m->snapshot.max_rtt_us) {
        m->snapshot.max_rtt_us = rtt_us;
    }
}

void metrics_record_bytes_sent(metrics_engine_t *m, uint64_t bytes) {
    if (!m || !m->enabled) return;
    m->snapshot.bytes_sent += bytes;
}

void metrics_record_bytes_received(metrics_engine_t *m, uint64_t bytes) {
    if (!m || !m->enabled) return;
    m->snapshot.bytes_received += bytes;
}

void metrics_record_service_detected(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.services_detected++;
}

void metrics_record_service_failed(metrics_engine_t *m) {
    if (!m || !m->enabled) return;
    m->snapshot.services_failed++;
}

void metrics_record_peak_concurrency(metrics_engine_t *m, uint32_t concurrent) {
    if (!m || !m->enabled) return;
    if (concurrent > m->snapshot.peak_concurrency) {
        m->snapshot.peak_concurrency = concurrent;
    }
}

void metrics_record_ports_scanned(metrics_engine_t *m, uint32_t count) {
    if (!m || !m->enabled) return;
    m->snapshot.ports_scanned += count;
}

void metrics_record_hosts_scanned(metrics_engine_t *m, uint32_t count) {
    if (!m || !m->enabled) return;
    m->snapshot.hosts_scanned += count;
}

metrics_snapshot_t metrics_get_snapshot(metrics_engine_t *m) {
    metrics_snapshot_t snap = {0};
    
    if (!m) return snap;
    
    snap = m->snapshot;
    clock_gettime(CLOCK_MONOTONIC, &snap.scan_end);
    
    return snap;
}

void metrics_reset(metrics_engine_t *m) {
    if (!m) return;
    
    memset(&m->snapshot, 0, sizeof(m->snapshot));
    m->snapshot.min_rtt_us = UINT64_MAX;
    clock_gettime(CLOCK_MONOTONIC, &m->snapshot.scan_start);
}

/* ====================================================================
   METRICS REPORTING
   ==================================================================== */

void metrics_print_table(const metrics_snapshot_t *snap) {
    if (!snap) return;
    
    metrics_derived_t derived = metrics_derive(snap);
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                    BLACKMAP SCAN METRICS                       ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Time Statistics:\n");
    printf("  Elapsed Time:           %.2f seconds\n", derived.elapsed_seconds);
    printf("  Start Time:             %lu sec, %lu nsec\n", snap->scan_start.tv_sec, snap->scan_start.tv_nsec);
    printf("  End Time:               %lu sec, %lu nsec\n", snap->scan_end.tv_sec, snap->scan_end.tv_nsec);
    
    printf("\nConnection Statistics:\n");
    printf("  Total Attempted:        %lu\n", snap->total_connections_attempted);
    printf("  Successful:             %lu\n", snap->total_connections_open);
    printf("  Closed:                 %lu\n", snap->total_connections_closed);
    printf("  Timeouts:               %lu\n", snap->total_timeouts);
    printf("  Errors:                 %lu\n", snap->total_errors);
    printf("  Success Rate:           %.2f%%\n", derived.success_rate_percent);
    printf("  Timeout Rate:           %.2f%%\n", derived.timeout_rate_percent);
    
    printf("\nRound-Trip Time (RTT):\n");
    printf("  Average:                %.2f ms\n", derived.avg_rtt_ms);
    printf("  Min:                    %.3f ms\n", snap->min_rtt_us / 1000.0);
    printf("  Max:                    %.3f ms\n", snap->max_rtt_us / 1000.0);
    printf("  Total Measurements:     %u\n", snap->rtt_measurements);
    
    printf("\nThroughput:\n");
    printf("  Ports Scanned:          %u\n", snap->ports_scanned);
    printf("  Hosts Scanned:          %u\n", snap->hosts_scanned);
    printf("  Ports/Second:           %.2f\n", derived.ports_per_second);
    printf("  Bytes Sent:             %lu\n", snap->bytes_sent);
    printf("  Bytes Received:         %lu\n", snap->bytes_received);
    
    printf("\nConcurrency:\n");
    printf("  Peak:                   %u\n", snap->peak_concurrency);
    printf("  Average:                %u\n", snap->avg_concurrency);
    
    printf("\nService Detection:\n");
    printf("  Detected:               %u\n", snap->services_detected);
    printf("  Failed:                 %u\n", snap->services_failed);
    
    printf("\n");
}

char* metrics_to_json(const metrics_snapshot_t *snap) {
    if (!snap) return NULL;
    
    metrics_derived_t derived = metrics_derive(snap);
    
    char buffer[4096];
    snprintf(buffer, sizeof(buffer),
        "{"
        "\"elapsed_seconds\":%.2f,"
        "\"total_connections_attempted\":%lu,"
        "\"total_connections_open\":%lu,"
        "\"total_timeouts\":%lu,"
        "\"total_errors\":%lu,"
        "\"avg_rtt_ms\":%.2f,"
        "\"min_rtt_us\":%lu,"
        "\"max_rtt_us\":%lu,"
        "\"ports_scanned\":%u,"
        "\"hosts_scanned\":%u,"
        "\"ports_per_second\":%.2f,"
        "\"success_rate_percent\":%.2f,"
        "\"timeout_rate_percent\":%.2f,"
        "\"bytes_sent\":%lu,"
        "\"bytes_received\":%lu,"
        "\"services_detected\":%u,"
        "\"services_failed\":%u,"
        "\"peak_concurrency\":%u"
        "}",
        derived.elapsed_seconds,
        snap->total_connections_attempted,
        snap->total_connections_open,
        snap->total_timeouts,
        snap->total_errors,
        derived.avg_rtt_ms,
        snap->min_rtt_us,
        snap->max_rtt_us,
        snap->ports_scanned,
        snap->hosts_scanned,
        derived.ports_per_second,
        derived.success_rate_percent,
        derived.timeout_rate_percent,
        snap->bytes_sent,
        snap->bytes_received,
        snap->services_detected,
        snap->services_failed,
        snap->peak_concurrency
    );
    
    char *result = (char*)malloc(strlen(buffer) + 1);
    if (result) {
        strcpy(result, buffer);
    }
    
    return result;
}

metrics_derived_t metrics_derive(const metrics_snapshot_t *snap) {
    metrics_derived_t derived = {0};
    
    if (!snap) return derived;
    
    // Calculate elapsed time
    uint64_t start_us = snap->scan_start.tv_sec * 1000000UL + snap->scan_start.tv_nsec / 1000;
    uint64_t end_us = snap->scan_end.tv_sec * 1000000UL + snap->scan_end.tv_nsec / 1000;
    uint64_t elapsed_us = end_us - start_us;
    derived.elapsed_seconds = elapsed_us / 1000000.0;
    
    // Ports per second
    if (derived.elapsed_seconds > 0.1) {
        derived.ports_per_second = snap->ports_scanned / derived.elapsed_seconds;
    }
    
    // Average RTT
    if (snap->rtt_measurements > 0) {
        derived.avg_rtt_ms = (snap->total_rtt_us / snap->rtt_measurements) / 1000.0;
    }
    
    // Success rate
    if (snap->total_connections_attempted > 0) {
        derived.success_rate_percent = 
            (100.0 * snap->total_connections_open) / snap->total_connections_attempted;
        derived.timeout_rate_percent = 
            (100.0 * snap->total_timeouts) / snap->total_connections_attempted;
    }
    
    return derived;
}
