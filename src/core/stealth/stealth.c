/* ====================================================================
   BLACKMAP 3.0 - STEALTH SYSTEM IMPLEMENTATION
   
   Behavior profiles for different scan scenarios
   ===================================================================== */

#include "blackmap3/stealth.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

stealth_config_t stealth_get_preset(stealth_level_t level) {
    stealth_config_t cfg = {0};
    
    cfg.level = level;
    cfg.base_timeout_ms = 5000;
    cfg.enable_exponential_backoff = 1;
    cfg.backoff_multiplier = 1.5f;
    cfg.backoff_max_delay_ms = 60000;
    
    switch (level) {
        case STEALTH_PERFORMANCE:
            // Level 0: All speed, no stealth
            cfg.max_concurrency_global = 256;
            cfg.max_concurrency_per_host = 64;
            cfg.jitter_percent = 0;
            cfg.enable_port_randomization = 0;
            cfg.enable_host_randomization = 0;
            cfg.max_packets_per_sec = 10000;
            cfg.delay_between_ports_ms = 0;
            cfg.delay_between_hosts_ms = 0;
            cfg.burst_size = 256;
            cfg.pause_after_burst_ms = 0;
            cfg.enable_rtt_awareness = 0;
            cfg.skip_version_detection = 0;
            cfg.batch_size_for_detection = 256;
            break;
            
        case STEALTH_BALANCED:
            // Level 1: Good speed with some randomization
            cfg.max_concurrency_global = 128;
            cfg.max_concurrency_per_host = 32;
            cfg.jitter_percent = 10;
            cfg.enable_port_randomization = 1;
            cfg.enable_host_randomization = 0;
            cfg.max_packets_per_sec = 5000;
            cfg.delay_between_ports_ms = 1;
            cfg.delay_between_hosts_ms = 5;
            cfg.burst_size = 128;
            cfg.pause_after_burst_ms = 50;
            cfg.enable_rtt_awareness = 1;
            cfg.skip_version_detection = 0;
            cfg.batch_size_for_detection = 128;
            break;
            
        case STEALTH_LOW_NOISE:
            // Level 2: Moderate concurrency, visible delays
            cfg.max_concurrency_global = 32;
            cfg.max_concurrency_per_host = 8;
            cfg.jitter_percent = 25;
            cfg.enable_port_randomization = 1;
            cfg.enable_host_randomization = 1;
            cfg.max_packets_per_sec = 1000;
            cfg.delay_between_ports_ms = 10;
            cfg.delay_between_hosts_ms = 50;
            cfg.burst_size = 32;
            cfg.pause_after_burst_ms = 500;
            cfg.enable_rtt_awareness = 1;
            cfg.skip_version_detection = 0;
            cfg.batch_size_for_detection = 32;
            break;
            
        case STEALTH_CONSERVATIVE:
            // Level 3: Low concurrency, adaptive pacing
            cfg.max_concurrency_global = 8;
            cfg.max_concurrency_per_host = 2;
            cfg.jitter_percent = 50;
            cfg.enable_port_randomization = 1;
            cfg.enable_host_randomization = 1;
            cfg.max_packets_per_sec = 100;
            cfg.delay_between_ports_ms = 100;
            cfg.delay_between_hosts_ms = 500;
            cfg.burst_size = 8;
            cfg.pause_after_burst_ms = 2000;
            cfg.enable_rtt_awareness = 1;
            cfg.skip_version_detection = 0;
            cfg.batch_size_for_detection = 8;
            break;
            
        case STEALTH_ULTRA_CONSERVATIVE:
            // Level 4: Minimal, very slow and stealthy
            cfg.max_concurrency_global = 1;
            cfg.max_concurrency_per_host = 1;
            cfg.jitter_percent = 80;
            cfg.enable_port_randomization = 1;
            cfg.enable_host_randomization = 1;
            cfg.max_packets_per_sec = 10;
            cfg.delay_between_ports_ms = 500;
            cfg.delay_between_hosts_ms = 2000;
            cfg.burst_size = 1;
            cfg.pause_after_burst_ms = 5000;
            cfg.enable_rtt_awareness = 1;
            cfg.skip_version_detection = 1;  // Skip banners
            cfg.batch_size_for_detection = 1;
            cfg.enable_exponential_backoff = 1;
            break;
    }
    
    return cfg;
}

stealth_config_t stealth_create_custom(
    uint32_t max_global_concurrency,
    uint32_t max_per_host_concurrency,
    uint32_t base_timeout_ms,
    int randomize_ports,
    int enable_jitter)
{
    stealth_config_t cfg = stealth_get_preset(STEALTH_BALANCED);
    
    cfg.max_concurrency_global = max_global_concurrency;
    cfg.max_concurrency_per_host = max_per_host_concurrency;
    cfg.base_timeout_ms = base_timeout_ms;
    cfg.enable_port_randomization = randomize_ports;
    cfg.jitter_percent = enable_jitter ? 20 : 0;
    
    return cfg;
}

stealth_ctx_t* stealth_init(stealth_config_t config) {
    stealth_ctx_t *ctx = (stealth_ctx_t*)malloc(sizeof(*ctx));
    if (!ctx) return NULL;
    
    ctx->config = config;
    ctx->burst_counter = 0;
    ctx->last_burst_time_us = 0;
    
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ctx->startup_time_us = (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    
    return ctx;
}

void stealth_free(stealth_ctx_t *ctx) {
    if (ctx) free(ctx);
}

uint32_t stealth_get_pre_connect_delay_us(stealth_ctx_t *ctx) {
    if (!ctx) return 0;
    
    uint32_t base_delay = ctx->config.delay_between_ports_ms * 1000;
    
    if (ctx->config.jitter_percent == 0) {
        return base_delay;
    }
    
    // Add random jitter
    uint32_t jitter_amount = (base_delay * ctx->config.jitter_percent) / 100;
    uint32_t random_jitter = (rand() % (jitter_amount + 1));
    
    return base_delay + random_jitter;
}

uint32_t stealth_get_post_connect_pause_us(stealth_ctx_t *ctx) {
    if (!ctx) return 0;
    
    ctx->burst_counter++;
    
    if (ctx->burst_counter < ctx->config.burst_size) {
        return 0;  // No pause within burst
    }
    
    // Hit burst limit, reset and return pause
    ctx->burst_counter = 0;
    return ctx->config.pause_after_burst_ms * 1000;
}

uint32_t stealth_get_adaptive_timeout_ms(
    stealth_ctx_t *ctx,
    uint64_t measured_rtt_us)
{
    if (!ctx || !ctx->config.enable_rtt_awareness) {
        return ctx->config.base_timeout_ms;
    }
    
    // Timeout = 2x RTT + base buffer
    uint32_t adaptive = (measured_rtt_us * 2) / 1000;
    
    // But never less than base timeout
    if (adaptive < ctx->config.base_timeout_ms) {
        return ctx->config.base_timeout_ms;
    }
    
    // Or more than 10x base
    if (adaptive > ctx->config.base_timeout_ms * 10) {
        return ctx->config.base_timeout_ms * 10;
    }
    
    return adaptive;
}

int stealth_should_detect_version(stealth_ctx_t *ctx, uint32_t open_port_count) {
    if (!ctx) return 1;
    if (ctx->config.skip_version_detection) return 0;
    
    // Only detect if we haven't hit batch limit
    return open_port_count <= ctx->config.batch_size_for_detection;
}

uint32_t stealth_get_backoff_delay_ms(stealth_ctx_t *ctx, uint32_t timeout_count) {
    if (!ctx || !ctx->config.enable_exponential_backoff) {
        return 0;
    }
    
    // exponential backoff: initial * multiplier^count
    uint32_t base = 500;
    uint32_t backoff = base;
    
    for (uint32_t i = 0; i < timeout_count; i++) {
        backoff = (uint32_t)((float)backoff * ctx->config.backoff_multiplier);
        if (backoff > ctx->config.backoff_max_delay_ms) {
            backoff = ctx->config.backoff_max_delay_ms;
            break;
        }
    }
    
    return backoff;
}

void stealth_randomize_ports(stealth_ctx_t *ctx, uint16_t *ports, uint32_t count) {
    if (!ctx || !ports || count == 0) return;
    if (!ctx->config.enable_port_randomization) return;
    
    // Fisher-Yates shuffle
    for (uint32_t i = count - 1; i > 0; i--) {
        uint32_t j = rand() % (i + 1);
        uint16_t tmp = ports[i];
        ports[i] = ports[j];
        ports[j] = tmp;
    }
}

const char* stealth_get_description(stealth_level_t level) {
    switch (level) {
        case STEALTH_PERFORMANCE:
            return "Performance: Maximum speed, no hiding";
        case STEALTH_BALANCED:
            return "Balanced: Good speed with randomization";
        case STEALTH_LOW_NOISE:
            return "Low Noise: Limited concurrency, visible delays";
        case STEALTH_CONSERVATIVE:
            return "Conservative: Low concurrency, adaptive pacing";
        case STEALTH_ULTRA_CONSERVATIVE:
            return "Ultra Conservative: Minimal, very slow and stealthy";
        default:
            return "Unknown";
    }
}
