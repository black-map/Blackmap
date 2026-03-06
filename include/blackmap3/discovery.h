#ifndef BLACKMAP3_HOST_DISCOVERY_ADVANCED_H
#define BLACKMAP3_HOST_DISCOVERY_ADVANCED_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * PROFESSIONAL HOST DISCOVERY MODULE
 * 
 * This module provides multiple host discovery strategies to determine if targets
 * are alive (reachable) before commencing port scanning. Strategies include:
 * 
 * 1. ICMP Echo Ping (requires root)
 * 2. TCP SYN Ping (requires root)
 * 3. TCP ACK Ping (requires root)
 * 4. TCP CONNECT Ping (no root required)
 * 5. UDP Ping (for specific UDP services)
 * 
 * Features:
 * - Multi-strategy fallback: attempts several methods if one fails
 * - Non-blocking event-driven I/O where possible
 * - Comprehensive port selection (not just port 80)
 * - Proper timeout handling
 * - Detailed logging and statistics
 * - Optional skip entirely (-Pn flag)
 */

typedef enum {
    DISCOVERY_METHOD_NONE = 0,        /* -Pn: Skip discovery entirely */
    DISCOVERY_METHOD_ICMP_ECHO = 1,   /* ICMP Echo Request/Reply */
    DISCOVERY_METHOD_TCP_SYN = 2,     /* TCP SYN/ACK */
    DISCOVERY_METHOD_TCP_ACK = 3,     /* TCP ACK/RST */
    DISCOVERY_METHOD_TCP_CONNECT = 4, /* TCP CONNECT (no root) */
    DISCOVERY_METHOD_UDP = 5,         /* UDP probes */
    DISCOVERY_METHOD_COMBINED = 6     /* Try multiple methods sequentially */
} discovery_method_t;

typedef enum {
    DISCOVERY_PROBE_ICMP = 0,
    DISCOVERY_PROBE_TCP_SYN = 1,
    DISCOVERY_PROBE_TCP_ACK = 2,
    DISCOVERY_PROBE_TCP_CONNECT = 3,
    DISCOVERY_PROBE_UDP = 4
} discovery_probe_type_t;

typedef struct {
    uint32_t total_probes_sent;
    uint32_t successful_probes;
    uint32_t failed_probes;
    uint32_t timeouts;
    uint32_t hosts_discovered_up;
    uint32_t duration_ms;
} discovery_stats_t;

/**
 * Single host discovery result
 */
typedef struct {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int family;  /* AF_INET or AF_INET6 */
    char addr_str[INET6_ADDRSTRLEN];
    char hostname[256];
    
    bool is_up;      /* Host responds to probes */
    uint32_t rtt_ms; /* Round-trip time for the successful probe */
    discovery_probe_type_t probe_method_used;  /* Which method succeeded */
} discovery_result_t;

/**
 * Configuration for host discovery process
 */
typedef struct {
    discovery_method_t method;     /* Which strategy to use */
    uint16_t *probe_ports;         /* Ports to use for TCP probes */
    uint16_t probe_port_count;     /* Number of probe ports */
    uint32_t timeout_ms;           /* Timeout per probe */
    uint32_t max_retries;          /* Max retries per probe */
    bool skip_discovery;           /* -Pn: treat all hosts as up */
    bool verbose;                  /* Detailed debug output */
} discovery_config_t;

/**
 * Initialize default discovery configuration
 */
discovery_config_t* discovery_config_create(void);

/**
 * Free discovery configuration
 */
void discovery_config_free(discovery_config_t *config);

/**
 * Discover a single host
 * 
 * @param config Discovery configuration
 * @param target The host address to probe
 * @param result Pointer to result structure to fill
 * @return 0 on success, -1 on error (result->is_up indicates success)
 */
int discovery_probe_host(const discovery_config_t *config, 
                        struct sockaddr_storage *target,
                        discovery_result_t *result);

/**
 * Discover multiple hosts with bulk operation
 * 
 * @param config Discovery configuration
 * @param targets Array of host addresses
 * @param target_count Number of targets
 * @param results Array to receive results (must be pre-allocated)
 * @param stats Pointer to statistics structure to fill
 * @return Number of hosts confirmed up
 */
int discovery_probe_hosts(const discovery_config_t *config,
                         struct sockaddr_storage *targets,
                         uint32_t target_count,
                         discovery_result_t *results,
                         discovery_stats_t *stats);

/**
 * TCP CONNECT ping (works without root)
 */
int discovery_tcp_connect_ping(struct sockaddr_storage *addr, 
                              uint16_t port,
                              uint32_t timeout_ms);

/**
 * TCP SYN ping (requires root)
 */
int discovery_tcp_syn_ping(struct sockaddr_storage *addr,
                          uint16_t port,
                          uint32_t timeout_ms);

/**
 * TCP ACK ping (requires root) 
 */
int discovery_tcp_ack_ping(struct sockaddr_storage *addr,
                          uint16_t port,
                          uint32_t timeout_ms);

/**
 * ICMP Echo ping (requires root)
 */
int discovery_icmp_echo_ping(struct sockaddr_storage *addr,
                            uint32_t timeout_ms);

/**
 * UDP probe (connectionless)
 */
int discovery_udp_probe(struct sockaddr_storage *addr,
                       uint16_t port,
                       uint32_t timeout_ms);

#endif /* BLACKMAP3_HOST_DISCOVERY_ADVANCED_H */
