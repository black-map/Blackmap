#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <arpa/inet.h>
#include "blackmap.h"
#include "engines.h"
#include "service.h"
#include "proxy.h"

static io_engine_t *current_engine = NULL;

int blackmap_init(void) {
    if (!g_config) {
        fprintf(stderr, "Error: Config not initialized\n");
        return -1;
    }
    
    /* Check for root privilege if required */
    if (g_config->require_root && geteuid() != 0) {
        fprintf(stderr, "Warning: BlackMap requires root privileges for raw socket operations\n");
        fprintf(stderr, "Some features may be unavailable. Use -Pn for TCP connect scan without root.\n");
        /* Non-fatal for now */
    }
    
    /* Select and initialize I/O engine */
    switch (g_config->io_engine) {
        case IO_ENGINE_URING:
            current_engine = engine_get_uring();
            break;
        case IO_ENGINE_XDP:
            current_engine = engine_get_xdp();
            break;
        case IO_ENGINE_EPOLL:
            current_engine = engine_get_epoll();
            break;
        case IO_ENGINE_SELECT:
        default:
            current_engine = engine_get_select();
            break;
    }
    
    if (!current_engine) {
        fprintf(stderr, "Error: Failed to get I/O engine\n");
        return -1;
    }
    
    if (g_config->debug) {
        printf("[*] Using I/O engine: %s\n", current_engine->name);
    }
    
    /* Initialize engine */
    if (current_engine->init() != 0) {
        fprintf(stderr, "Error: Failed to initialize I/O engine\n");
        return -1;
    }
    
    /* Enforce proxy mode if needed */
    enforce_proxy_mode();    
    /* Enforce proxy mode if needed */
    enforce_proxy_mode();
    
    /* Check for Tor/SOCKS5 and configure accordingly */
    if (detect_tor_mode()) {
        if (g_config->verbosity > 0) {
            printf("[*] Tor/SOCKS5 proxy detected - optimizing for anonymity\n");
        }
        enable_tor_mode();
        if (g_config->debug) {
            validate_tor_anonymity_level();
        }
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] BlackMap v%s initialized\n", BLACKMAP_VERSION);
    }
    
    return 0;
}

int blackmap_run(void) {
    if (!g_config) {
        fprintf(stderr, "Error: Config not initialized\n");
        return -1;
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] Starting BlackMap scan\n");
        printf("[*] Target(s): %s\n", g_config->targets_str);
        printf("[*] Scan type: %d\n", g_config->scan_type);
        printf("[*] Ports to scan: %u\n", g_config->num_ports);
        printf("[*] Timeout: %ums\n", g_config->timeout_ms);
    }
    
    /* Parse targets */
    uint32_t ip_start, ip_end;
    if (parse_ipv4_target(g_config->targets_str, &ip_start, &ip_end) != 0) {
        fprintf(stderr, "[-] Failed to parse targets\n");
        return -1;
    }
    
    uint32_t total_hosts = ip_end - ip_start + 1;
    uint32_t total_ports = g_config->num_ports ? g_config->num_ports : 1000;
    uint64_t total_probes = (uint64_t)total_hosts * total_ports;
    
    if (g_config->verbosity > 0) {
        printf("[*] Hosts to scan: %u\n", total_hosts);
        printf("[*] Total probes: %lu\n", (unsigned long)total_probes);
    }
    
    /* Check if we need TCP CONNECT instead of SYN */
    if (g_config->scan_type == SCAN_TYPE_SYN && geteuid() != 0) {
        if (g_config->verbosity > 0) {
            printf("[!] Not running as root - switching to TCP CONNECT scan\n");
        }
        g_config->scan_type = SCAN_TYPE_CONNECT;
    }
    
    if (g_config->version_detection) {
        if (g_config->scan_type == SCAN_TYPE_SYN) {
            g_config->scan_type = SCAN_TYPE_CONNECT;
            if (g_config->verbosity > 0) {
                printf("[!] Version detection enabled - switching to TCP CONNECT scan\n");
            }
        }
    }
    
    int open_ports = 0;
    int closed_ports = 0;
    int filtered_ports = 0;
    
    /* Scan hosts */
    for (uint32_t host = ip_start; host <= ip_end; host++) {
        int host_open = 0;
        
        /* Scan ports */
        for (uint32_t i = 0; i < g_config->num_ports; i++) {
            uint16_t port = g_config->ports[i];
            int state = PORT_UNKNOWN;
            
            /* Perform scan based on type */
            if (g_config->scan_type == SCAN_TYPE_CONNECT) {
                state = tcp_connect_scan(host, port, g_config->timeout_ms);
            } else if (g_config->scan_type == SCAN_TYPE_SYN) {
                state = tcp_syn_scan_stub(host, port);
            } else {
                /* Default to CONNECT */
                state = tcp_connect_scan(host, port, g_config->timeout_ms);
            }
            
            /* Track results */
            if (state == PORT_OPEN) {
                open_ports++;
                host_open++;
                
                if (g_config->verbosity > 1 || g_config->debug) {
                    struct in_addr addr;
                    addr.s_addr = host;
                    printf("[*] %s:%u - OPEN\n", inet_ntoa(addr), port);
                }
                
                if (g_config->version_detection) {
                    port_info_t info;
                    memset(&info, 0, sizeof(info));
                    detect_service(host, port, &info);
                }
            } else if (state == PORT_CLOSED) {
                closed_ports++;
            } else if (state == PORT_FILTERED) {
                filtered_ports++;
            }
            
            /* Rate limiting */
            if (g_config->max_rate > 0) {
                uint32_t delay_us = 1000000 / g_config->max_rate;
                usleep(delay_us);
            }
        }
        
        if (host_open > 0 && g_config->verbosity > 0) {
            struct in_addr addr;
            addr.s_addr = host;
            printf("[+] Host %s: %d open port(s)\n", inet_ntoa(addr), host_open);
        }
    }
    
    /* Print summary */
    printf("\n[+] Scan Complete!\n");
    printf("[*] Open ports: %d\n", open_ports);
    printf("[*] Closed ports: %d\n", closed_ports);
    printf("[*] Filtered ports: %d\n", filtered_ports);
    
    return 0;
}

void blackmap_cleanup(void) {
    if (current_engine && current_engine->cleanup) {
        current_engine->cleanup();
    }
    
    if (g_config) {
        if (g_config->payload) {
            free(g_config->payload);
        }
        if (g_config->ports) {
            free(g_config->ports);
        }
        free(g_config);
        g_config = NULL;
    }
    
    if (g_config && g_config->verbosity > 0) {
        printf("[+] BlackMap cleanup completed\n");
    }
}
