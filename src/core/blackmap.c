#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
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
    
    int total_open = 0;
    int total_closed = 0;
    int total_filtered = 0;
    int hosts_up = 0;
    struct timeval tv_global_start, tv_global_end;
    gettimeofday(&tv_global_start, NULL);
    time_t scan_start = time(NULL);
    
    /* Print header */
    printf("\nStarting BlackMap v%s ( https://github.com/Brian-Rojo/Blackmap ) at %s", BLACKMAP_VERSION, ctime(&scan_start));
    
    /* Allocate space for results */
    host_info_t *results = malloc(sizeof(host_info_t) * (total_hosts));
    if (!results) {
        fprintf(stderr, "[-] Memory allocation failed\n");
        return -1;
    }
    memset(results, 0, sizeof(host_info_t) * total_hosts);
    
    /* Allocate ports array for each host */
    for (uint32_t h = 0; h < total_hosts; h++) {
        results[h].ports = malloc(sizeof(port_info_t) * g_config->num_ports);
        if (!results[h].ports) {
            fprintf(stderr, "[-] Memory allocation failed\n");
            return -1;
        }
        memset(results[h].ports, 0, sizeof(port_info_t) * g_config->num_ports);
    }
    
    /* Scan hosts */
    for (uint32_t host = ip_start; host <= ip_end; host++) {
        uint32_t host_idx = host - ip_start;
        host_info_t *h = &results[host_idx];
        h->ip4.s_addr = host;
        h->state = HOST_DOWN;
        
        struct timeval tv_start, tv_end;
        gettimeofday(&tv_start, NULL);
        
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
            
            /* Store port result */
            h->ports[h->num_ports].port = port;
            h->ports[h->num_ports].state = state;
            h->num_ports++;
            
            /* Track results */
            if (state == PORT_OPEN) {
                h->state = HOST_UP;
                total_open++;
                
                if (g_config->version_detection) {
                    port_info_t info;
                    memset(&info, 0, sizeof(info));
                    detect_service(host, port, &info);
                    /* copy detected service and version if available */
                    if (info.service[0]) {
                        strncpy(h->ports[h->num_ports - 1].service, info.service,
                                sizeof(h->ports[h->num_ports - 1].service) - 1);
                    }
                    if (info.version[0]) {
                        strncpy(h->ports[h->num_ports - 1].version, info.version,
                                sizeof(h->ports[h->num_ports - 1].version) - 1);
                    }
                }
            } else if (state == PORT_CLOSED) {
                total_closed++;
            } else if (state == PORT_FILTERED) {
                total_filtered++;
            }
            
            /* Rate limiting */
            if (g_config->max_rate > 0) {
                uint32_t delay_us = 1000000 / g_config->max_rate;
                usleep(delay_us);
            }
        }
        
        gettimeofday(&tv_end, NULL);
        h->rtt_avg_us = ((tv_end.tv_sec - tv_start.tv_sec) * 1000000) + 
                        (tv_end.tv_usec - tv_start.tv_usec);
        
        if (h->state == HOST_UP) {
            hosts_up++;
        }
    }
    
    /* Print results - only hosts that are UP */
    for (uint32_t i = 0; i < total_hosts; i++) {
        host_info_t *h = &results[i];
        
        if (h->state == HOST_UP) {
            printf("Nmap scan report for %s\n", inet_ntoa(h->ip4));
            printf("Host is up (%.4fs latency).\n", (float)h->rtt_avg_us / 1000000.0);
            
            /* Count open and closed ports */
            int open_count = 0;
            for (uint32_t j = 0; j < h->num_ports; j++) {
                if (h->ports[j].state == PORT_OPEN) {
                    open_count++;
                }
            }
            
            if (open_count > 0) {
                int filtered_count = 0;
                for (uint32_t k = 0; k < h->num_ports; k++) {
                    if (h->ports[k].state == PORT_FILTERED) filtered_count++;
                }
                int closed_count = h->num_ports - open_count - filtered_count;
                printf("Not shown: %d closed, %d filtered tcp ports\n", closed_count, filtered_count);
                printf("PORT      STATE SERVICE/VERSION\n");
                
                /* Print stored open ports (no re-scan) */
                for (uint32_t j = 0; j < h->num_ports; j++) {
                    if (h->ports[j].state == PORT_OPEN) {
                        uint16_t port = h->ports[j].port;
                        const char *service = h->ports[j].service[0] ? h->ports[j].service : "unknown";
                        const char *version = h->ports[j].version;
                        if (version && version[0]) {
                            printf("%d/tcp    open  %s %s\n", port, service, version);
                        } else {
                            printf("%d/tcp    open  %s\n", port, service);
                        }
                    }
                }
            } else {
                int filtered_count = 0;
                for (uint32_t k = 0; k < h->num_ports; k++) {
                    if (h->ports[k].state == PORT_FILTERED) filtered_count++;
                }
                printf("All %u scanned ports on %s are in ignored states.\n",
                       g_config->num_ports, inet_ntoa(h->ip4));
                printf("Not shown: %u closed, %d filtered tcp ports\n", g_config->num_ports, filtered_count);
            }
            printf("\n");
        }
    }
    
    /* Clean up allocated memory */
    for (uint32_t h = 0; h < total_hosts; h++) {
        if (results[h].ports) {
            free(results[h].ports);
        }
    }
    
    /* Print summary */
    gettimeofday(&tv_global_end, NULL);
    double elapsed = ((tv_global_end.tv_sec - tv_global_start.tv_sec) * 1000.0) +
                     ((tv_global_end.tv_usec - tv_global_start.tv_usec) / 1000.0);
    
    printf("BlackMap done at %s", ctime(&scan_start));
    printf("BlackMap done: %u IP address(es) (%u host up) scanned in %.2f seconds\n", 
           total_hosts, hosts_up, elapsed / 1000.0);
    
    /* Print metrics if requested */
    if (g_config->print_stats && g_config->metrics_format[0] != '\0') {
        if (strcmp(g_config->metrics_format, "json") == 0) {
            printf("\n");
            printf("{\"metrics\": {\n");
            printf("  \"elapsed_seconds\": %.2f,\n", elapsed / 1000.0);
            printf("  \"total_hosts\": %u,\n", total_hosts);
            printf("  \"hosts_up\": %u,\n", hosts_up);
            printf("  \"open_ports\": %d,\n", total_open);
            printf("  \"closed_ports\": %d,\n", total_closed);
            printf("  \"filtered_ports\": %d,\n", total_filtered);
            printf("  \"total_probes\": %lu\n", (unsigned long)total_probes);
            printf("}}\n");
        } else {
            /* table format (default) */
            printf("\n");
            printf("=== METRICS ===\n");
            printf("Elapsed:        %.2f seconds\n", elapsed / 1000.0);
            printf("Hosts scanned:  %u\n", total_hosts);
            printf("Hosts up:       %u\n", hosts_up);
            printf("Open ports:     %d\n", total_open);
            printf("Closed ports:   %d\n", total_closed);
            printf("Filtered ports: %d\n", total_filtered);
            printf("Total probes:   %lu\n", (unsigned long)total_probes);
            if (elapsed > 0) {
                double probes_per_sec = total_probes / (elapsed / 1000.0);
                printf("Throughput:     %.0f probes/sec\n", probes_per_sec);
            }
            printf("===============\n\n");
        }
    }
    
    free(results);
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
