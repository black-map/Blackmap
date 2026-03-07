#ifndef BLACKMAP3_STEALTH_H
#define BLACKMAP3_STEALTH_H

#include <stdint.h>

/* ====================================================================
   STEALTH SYSTEM 3.0 - Behavior profiles
   
   Level 0: Performance   - Max speed, no hiding
   Level 1: Balanced      - Random ordering + slight jitter
   Level 2: Low Noise     - Limited concurrency + delays
   Level 3: Conservative  - Burst control + adaptive pacing
   Level 4: Ultra Conservative - Minimal concurrency + custom pauses
   
   This is about BEHAVIOR, not evasion:
   - No raw socket manipulation
   - No fragmentation tricks
   - No IDS-specific evasion
   - Just intelligent timing and pacing
   ==================================================================== */

typedef enum {
    STEALTH_PERFORMANCE       = 0,
    STEALTH_BALANCED          = 1,
    STEALTH_LOW_NOISE         = 2,
    STEALTH_CONSERVATIVE      = 3,
    STEALTH_ULTRA_CONSERVATIVE = 4
} stealth_level_t;

typedef struct {
    stealth_level_t level;
    
    // Global concurrency settings
    uint32_t max_concurrency_global;
    uint32_t max_concurrency_per_host;
    
    // Timing and jitter
    uint32_t base_timeout_ms;
    uint32_t jitter_percent;           // 0-100, randomness in timing
    int enable_port_randomization;     // Randomize port scan order
    int enable_host_randomization;     // Randomize host order
    
    // Rate limiting
    uint32_t max_packets_per_sec;      // Throttle rate
    uint32_t delay_between_ports_ms;   // Delay between probes
    uint32_t delay_between_hosts_ms;   // Delay when switching hosts
    
    // Burst control
    uint32_t burst_size;               // How many connections in one burst
    uint32_t pause_after_burst_ms;     // Pause after burst
    
    // RTT adaptation
    int enable_rtt_awareness;          // Adjust timing based on RTT
    
    // Service detection strategy
    int skip_version_detection;        // Don't grab banners
    int batch_size_for_detection;      // How many open ports to batch
    
    // Backoff on errors
    int enable_exponential_backoff;    // Slow down on timeouts
    float backoff_multiplier;          // Backoff factor (1.5x, 2.0x, etc)
    uint32_t backoff_max_delay_ms;     // Max backoff delay cap
} stealth_config_t;

/* ====================================================================
   STEALTH CONFIG PRESETS
   ==================================================================== */

// Get preset configuration for a stealth level
stealth_config_t stealth_get_preset(stealth_level_t level);

// Customize a preset
stealth_config_t stealth_create_custom(
    uint32_t max_global_concurrency,
    uint32_t max_per_host_concurrency,
    uint32_t base_timeout_ms,
    int randomize_ports,
    int enable_jitter
);

/* ====================================================================
   STEALTH SYSTEM API
   ==================================================================== */

typedef struct {
    stealth_config_t config;
    uint32_t burst_counter;      // Packets sent in current burst
    uint64_t last_burst_time_us; // Timestamp of last burst
    uint64_t startup_time_us;    // Scan startup for rate limiting
} stealth_ctx_t;

// Initialize stealth context with configuration
stealth_ctx_t* stealth_init(stealth_config_t config);

// Free stealth context
void stealth_free(stealth_ctx_t *ctx);

// Before connecting: apply delays/jitter based on stealth level
// microseconds to wait before attempting connection
uint32_t stealth_get_pre_connect_delay_us(stealth_ctx_t *ctx);

// After connection attempt: check if we need to pause (burst control)
uint32_t stealth_get_post_connect_pause_us(stealth_ctx_t *ctx);

// Calculate adaptive timeout based on RTT
uint32_t stealth_get_adaptive_timeout_ms(
    stealth_ctx_t *ctx,
    uint64_t measured_rtt_us
);

// Should we attempt version detection on this port?
int stealth_should_detect_version(stealth_ctx_t *ctx, uint32_t open_port_count);

// Apply backoff when timeout occurs
uint32_t stealth_get_backoff_delay_ms(stealth_ctx_t *ctx, uint32_t timeout_count);

// Randomize port order if needed
void stealth_randomize_ports(stealth_ctx_t *ctx, uint16_t *ports, uint32_t count);

// Get description of current stealth level
const char* stealth_get_description(stealth_level_t level);

#endif // BLACKMAP3_STEALTH_H
