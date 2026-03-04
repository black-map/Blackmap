#ifndef BLACKMAP_H
#define BLACKMAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include <sys/queue.h>
#include <netinet/in.h>

/* Version */
#define BLACKMAP_VERSION "1.0.0"
#define BLACKMAP_VERSION_MAJOR 1
#define BLACKMAP_VERSION_MINOR 0
#define BLACKMAP_VERSION_PATCH 0

/* Constants */
#define MAX_TARGETS 10000000
#define MAX_PORTS 65536
#define MAX_THREADS 256
#define MAX_DECOYS 20
#define SCAN_BUFFER_SIZE 65536
#define PACKET_BUFFER_SIZE 65536

/* IO Engine Types */
typedef enum {
    IO_ENGINE_SELECT = 1,
    IO_ENGINE_EPOLL = 2,
    IO_ENGINE_URING = 3,
    IO_ENGINE_XDP = 4,
    IO_ENGINE_EBPF = 5
} io_engine_type_t;

/* Scan Types */
typedef enum {
    SCAN_TYPE_CONNECT = 1,      /* -sT */
    SCAN_TYPE_SYN = 2,          /* -sS */
    SCAN_TYPE_FIN = 3,          /* -sF */
    SCAN_TYPE_NULL = 4,         /* -sN */
    SCAN_TYPE_XMAS = 5,         /* -sX */
    SCAN_TYPE_ACK = 6,          /* -sA */
    SCAN_TYPE_WINDOW = 7,       /* -sW */
    SCAN_TYPE_MAIMON = 8,       /* -sM */
    SCAN_TYPE_IDLE = 9,         /* -sI */
    SCAN_TYPE_UDP = 10,         /* -sU */
    SCAN_TYPE_SCTP_INIT = 11,   /* -sY */
    SCAN_TYPE_SCTP_COOKIE = 12, /* -sZ */
    SCAN_TYPE_IP_PROTO = 13,    /* -sO */
    SCAN_TYPE_PING = 14         /* -sP */
} scan_type_t;

/* Port States */
typedef enum {
    PORT_UNKNOWN = 0,
    PORT_OPEN = 1,
    PORT_CLOSED = 2,
    PORT_FILTERED = 3,
    PORT_UNFILTERED = 4,
    PORT_OPEN_FILTERED = 5,
    PORT_CLOSED_FILTERED = 6
} port_state_t;

/* Host States */
typedef enum {
    HOST_UNKNOWN = 0,
    HOST_UP = 1,
    HOST_DOWN = 2
} host_state_t;

/* Timing Templates */
typedef enum {
    TIMING_PARANOID = 0,     /* -T0 */
    TIMING_SNEAKY = 1,       /* -T1 */
    TIMING_POLITE = 2,       /* -T2 */
    TIMING_NORMAL = 3,       /* -T3 */
    TIMING_AGGRESSIVE = 4,   /* -T4 */
    TIMING_INSANE = 5        /* -T5 */
} timing_template_t;

/* Port Information */
typedef struct {
    uint16_t port;
    port_state_t state;
    char protocol[16];        /* tcp, udp, sctp */
    char service[32];
    char version[256];
    char ostype[32];
    uint32_t confidence;      /* 0-100 */
    char banner[512];
} port_info_t;

/* Host Information */
typedef struct {
    struct in_addr ip4;
    struct in6_addr ip6;
    bool is_ipv6;
    host_state_t state;
    char hostname[256];
    char os[256];
    uint32_t os_confidence;
    uint32_t uptime_seconds;
    uint32_t last_boot;
    
    port_info_t *ports;
    uint32_t num_ports;
    
    /* Timing */
    struct timespec first_probe;
    struct timespec last_response;
    uint32_t rtt_min_us;
    uint32_t rtt_max_us;
    uint32_t rtt_avg_us;
} host_info_t;

/* Configuration */
typedef struct {
    char targets_str[4096];
    char target_file[512];
    char output_file[512];
    
    io_engine_type_t io_engine;
    scan_type_t scan_type;
    timing_template_t timing;
    
    uint16_t *ports;
    uint32_t num_ports;
    
    uint32_t min_rate;
    uint32_t max_rate;
    uint32_t scan_delay_ms;
    uint32_t max_scan_delay_ms;
    uint32_t timeout_ms;
    uint32_t retries;
    
    /* MTU and fragmentation */
    uint16_t mtu;
    bool send_eth;
    bool send_ip;
    
    /* Decoys */
    struct in_addr decoys[MAX_DECOYS];
    uint32_t num_decoys;
    char spoof_mac[18];
    uint16_t source_port;
    
    /* Data payload */
    uint8_t *payload;
    uint32_t payload_len;
    
    /* Personality/evasion */
    char personality[32];
    uint8_t ttl;
    uint16_t window;
    uint16_t mss;
    bool sack_permitted;
    bool tcp_timestamps;
    uint8_t wscale;
    
    /* Version detection */
    bool version_detection;
    uint32_t version_intensity;
    bool version_all;
    
    /* OS Fingerprinting */
    bool os_detection;
    
    /* Script scanning */
    bool script_scan;
    char script_names[512];
    char script_args[1024];
    uint32_t script_timeout_ms;
    
    /* Proxy compatibility */
    int proxy_compat_mode;
    bool proxy_enforced;
    int dns_mode; /* 0=local, 1=proxy, 2=none */
    
    /* Stealth */
    bool slow_stealth;
    
    /* Output formats */
    bool output_normal;
    bool output_xml;
    bool output_grep;
    bool output_json;
    bool output_sqlite;
    bool output_html;
    bool output_markdown;
    
    /* Statistics */
    bool print_stats;
    uint32_t stats_interval_ms;
    
    /* Verbosity */
    int verbosity;
    bool debug;
    
    /* Threading */
    uint32_t num_threads;
    
    /* Randomization */
    bool randomize_hosts;
    
    /* Privileges check */
    bool require_root;
} blackmap_config_t;

/* Global config */
extern blackmap_config_t *g_config;

/* Core functions */
int blackmap_init(void);
int blackmap_run(void);
void blackmap_cleanup(void);

/* Target parsing */
int parse_ipv4_target(const char *target, uint32_t *ip_start, uint32_t *ip_end);
int count_targets(const char *target_str);

/* Port parsing */
int parse_ports(const char *port_spec);
uint16_t service_to_port(const char *service);

/* Scanning */
int tcp_connect_scan(uint32_t target_ip, uint16_t port, int timeout_ms);
int tcp_syn_scan_stub(uint32_t target_ip, uint16_t port);

/* Tor/Proxy support */
int detect_tor_mode(void);
int enable_tor_mode(void);
int check_tor_connection(void);
int enable_tor_circuit_rotation(int rotate_every_hosts);
int validate_tor_anonymity_level(void);

#endif /* BLACKMAP_H */
