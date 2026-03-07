#ifndef BLACKMAP3_ANALYSIS_H
#define BLACKMAP3_ANALYSIS_H

#include <stdint.h>
#include <stddef.h>

/* ====================================================================
   ANALYSIS ENGINE 3.0 (Rust) - Service fingerprinting
   
   Robust banner parsing with:
   - Structured signature database
   - Probabilistic confidence scoring
   - Separate: protocol, implementation, version
   - Fingerprint caching
   - Zero-copy FFI where possible
   ==================================================================== */

typedef struct {
    char *service;           // Protocol (HTTP, SSH, FTP, etc)
    char *implementation;    // Specific software (Apache, OpenSSH, etc)
    char *version;           // Version string
    uint8_t confidence;      // 0-100 confidence score
    char *banner;            // Raw banner (may be truncated)
    size_t banner_len;
    
    // Metadata
    char **extra_fields;     // Key-value pairs (NULL terminated)
    size_t extra_fields_count;
} service_info_t;

/* ====================================================================
   ANALYSIS FFI API (Called from C, implemented in Rust)
   ==================================================================== */

// Analyze raw banner and extract service information
// Input: raw banner bytes
// Output: service_info_t with parsed data
// Returns: allocated service_info_t, must be freed with analysis_free_service_info()
service_info_t* analysis_parse_banner(
    const uint8_t *banner,
    size_t banner_len
);

// Free service info struct
void analysis_free_service_info(service_info_t *info);

// Get service from port number (common services)
// Returns: "unknown" for unknown ports
const char* analysis_get_service_from_port(uint16_t port);

// Validate if banner contains typical protocol markers
// Returns: 1 if likely valid, 0 if garbage
int analysis_is_likely_valid_banner(
    const uint8_t *banner,
    size_t banner_len
);

// Serialize service_info_t to JSON
// Returns: allocated JSON string, must be freed with analysis_free_string()
char* analysis_service_to_json(const service_info_t *info);

// Free strings allocated by Rust
void analysis_free_string(char *s);

/* ====================================================================
   RESULT STRUCTURES FOR C LAYER
   ==================================================================== */

typedef struct {
    uint16_t port;
    int is_open;
    uint64_t connect_rtt_us;
    service_info_t *service_info;  // NULL if not detected or closed
} port_result_t;

typedef struct {
    char *ip;
    int is_up;
    uint64_t avg_rtt_us;
    port_result_t *ports;
    uint32_t port_count;
    unsigned long scan_start_time;
    unsigned long scan_end_time;
} host_result_t;

/* ====================================================================
   METRICS FOR ANALYSIS LAYER
   ==================================================================== */

typedef struct {
    uint64_t total_banners_analyzed;
    uint64_t successful_detections;
    uint64_t failed_detections;
    float average_confidence;
    uint64_t cache_hits;
    uint64_t cache_misses;
} analysis_metrics_t;

// Get metrics from analysis engine
analysis_metrics_t analysis_get_metrics(void);

// Clear analysis cache
void analysis_clear_cache(void);

#endif // BLACKMAP3_ANALYSIS_H
