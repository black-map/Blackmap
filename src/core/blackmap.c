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
#include "blackmap3/discovery.h"
#include "blackmap3/host_discovery.h"
#include "blackmap3/scheduler.h"
#include "blackmap3/network.h"
#include "engines.h"
#include "service.h"
#include "proxy.h"

static io_engine_t *current_engine = NULL;

/* global network engine pointer (declared in blackmap.h) */
network_engine_t *g_engine = NULL;

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
    /* expose engine pointer for other subsystems */
    g_engine = network_engine_init(g_config->num_threads,
                                   g_config->max_rate > 0 ? g_config->max_rate : 1024,
                                   g_config->timeout_ms);
    if (!g_engine) {
        fprintf(stderr, "Error: Could not initialize network engine\n");
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
        if (g_config->verbosity > 1) {
            printf("[DEBUG] Scan configuration:\n");
            printf("[DEBUG]   - IO Engine: %s\n", current_engine->name);
            printf("[DEBUG]   - Timeout: %ums\n", g_config->timeout_ms);
            printf("[DEBUG]   - Max rate: %u pps\n", g_config->max_rate);
            printf("[DEBUG]   - Timing level: %d\n", g_config->timing);
        }
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
        if (g_config->verbosity > 1) {
            printf("[DEBUG] Host discovery: %s\n", g_config->skip_ping ? "disabled (-Pn)" : "enabled");
        }
    }
    
    /* Build target list (expanding ranges/DNS names).  The function handles
       IPv4 and will return one entry per resolved address. */
    host_entry_t *hosts = NULL;
    uint32_t num_hosts = 0;
    
    if (g_config->verbosity > 1) {
        printf("[DEBUG] Starting target resolution phase\n");
    }
    
    if (build_host_list(g_config->targets_str, &hosts, &num_hosts) != 0 || num_hosts == 0) {
        fprintf(stderr, "[-] Failed to parse or resolve targets\n");
        return -1;
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] Resolved targets into %u host(s)\n", num_hosts);
    }

    /* Ensure we have a port list; parse default if user did not specify -p */
    if (g_config->num_ports == 0) {
        if (parse_ports(NULL) != 0) {
            fprintf(stderr, "[-] Failed to set default port list\n");
            host_list_free(hosts, num_hosts);
            return -1;
        }
    }

    uint32_t total_hosts = num_hosts;
    uint32_t total_ports = g_config->num_ports;
    uint64_t total_probes = (uint64_t)total_hosts * total_ports;

    if (g_config->verbosity > 0) {
        printf("[*] Hosts to scan: %u\n", total_hosts);
        printf("[*] Total probes: %lu\n", (unsigned long)total_probes);
    }

    /* Perform host discovery stage using the new discovery module */
    if (g_config->verbosity > 0) {
        printf("[*] Starting host discovery phase\n");
    }
    
    int discovered_up = 0;
    if (g_config->skip_ping) {
        /* -Pn: Skip discovery, treat all as up */
        if (g_config->verbosity > 0) {
            printf("[+] -Pn specified: treating all %u host(s) as alive (skipping discovery)\n", num_hosts);
        }
        for (uint32_t i = 0; i < num_hosts; i++) {
            hosts[i].state = HOST_UP;
            discovered_up++;
        }
    } else {
        /* Use the new professional host discovery system */
        discovery_config_t *disc_config = discovery_config_create();
        if (disc_config) {
            disc_config->skip_discovery = false;
            disc_config->timeout_ms = g_config->timeout_ms;
            disc_config->verbose = (g_config->verbosity > 1);  /* Verbose if -vv or higher */
            
            discovery_stats_t stats;
            discovery_result_t *disc_results = calloc(num_hosts, sizeof(discovery_result_t));
            
            /* Create temporary array of sockaddr_storage from host_entry_t */
            struct sockaddr_storage *targets = calloc(num_hosts, sizeof(struct sockaddr_storage));
            if (disc_results && targets) {
                for (uint32_t i = 0; i < num_hosts; i++) {
                    targets[i] = hosts[i].addr;
                }
                
                if (g_config->verbosity > 1) {
                    printf("[DEBUG] Sending discovery probes to %u host(s)...\n", num_hosts);
                }
                
                discovered_up = discovery_probe_hosts(disc_config, targets,
                    num_hosts, disc_results, &stats);
                
                if (g_config->verbosity > 0) {
                    printf("[*] Host discovery complete: %u host(s) up (took %ums)\n",
                           discovered_up, stats.duration_ms);
                    if (g_config->verbosity > 1) {
                        printf("[DEBUG] Discovery stats: %u probes sent, %u successful, %u failed\n",
                               stats.total_probes_sent, stats.successful_probes, stats.failed_probes);
                    }
                }
                
                /* Update host states based on discovery results */
                for (uint32_t i = 0; i < num_hosts; i++) {
                    if (disc_results[i].is_up) {
                        hosts[i].state = HOST_UP;
                        if (g_config->verbosity > 1) {
                            printf("[DEBUG] Host %s marked UP (method: %d, RTT: %ums)\n",
                                   hosts[i].addr_str, disc_results[i].probe_method_used,
                                   disc_results[i].rtt_ms);
                        }
                    } else {
                        hosts[i].state = HOST_DOWN;
                        if (g_config->verbosity > 1) {
                            printf("[DEBUG] Host %s marked DOWN (no response to probes)\n",
                                   hosts[i].addr_str);
                        }
                    }
                }
                
                free(targets);
                free(disc_results);
            } else {
                fprintf(stderr, "[-] Memory allocation failed for discovery\n");
                if (targets) free(targets);
                if (disc_results) free(disc_results);
                discovered_up = 0;
            }
            discovery_config_free(disc_config);
        } else {
            fprintf(stderr, "[-] Failed to create discovery configuration\n");
            discovered_up = 0;
        }
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
    
    /* Initialize concurrent scanning components */
    scheduler_t *sched = scheduler_create(
        g_config->num_threads,  // max_concurrency_global
        g_config->num_threads > 4 ? g_config->num_threads / 4 : 1,  // max_concurrency_per_host (at least 1)
        SCHEDULE_MODE_COMMON  // prioritize common ports
    );
    if (!sched) {
        fprintf(stderr, "[-] Failed to create scheduler\n");
        return -1;
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] Scheduler initialized with %u max concurrency\n", g_config->num_threads);
    }

    network_engine_t *net_engine = network_engine_init(
        g_config->num_threads,  // max_concurrency_global
        g_config->num_threads > 4 ? g_config->num_threads / 4 : 1,  // max_concurrency_per_host
        g_config->timeout_ms  // default_timeout_ms
    );
    if (!net_engine) {
        fprintf(stderr, "[-] Failed to create network engine\n");
        scheduler_free(sched);
        return -1;
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] Network engine initialized with %u max concurrency\n", g_config->num_threads);
    }

    /* Create scan plan */
    scan_plan_t plan;
    memset(&plan, 0, sizeof(plan));
    plan.num_hosts = num_hosts;
    plan.num_total_tasks = num_hosts * g_config->num_ports;
    plan.default_probe = (g_config->scan_type == SCAN_TYPE_CONNECT) ? PROBE_TCP_CON : PROBE_TCP_SYN;

    // Allocate arrays
    plan.ports = malloc(sizeof(uint32_t*) * num_hosts);
    plan.port_counts = malloc(sizeof(uint16_t) * num_hosts);
    plan.target_ips = malloc(sizeof(char*) * num_hosts);

    if (!plan.ports || !plan.port_counts || !plan.target_ips) {
        fprintf(stderr, "[-] Memory allocation failed for scan plan\n");
        scheduler_free(sched);
        network_engine_cleanup(net_engine);
        return -1;
    }

    // Populate scan plan
    for (uint32_t hidx = 0; hidx < num_hosts; hidx++) {
        host_entry_t *entry = &hosts[hidx];
        host_info_t *h = &results[hidx];

        // Copy address/hostname for reporting
        h->state = entry->state == HOST_UP ? HOST_UP : HOST_DOWN;
        if (entry->addr.ss_family == AF_INET) {
            h->ip4 = ((struct sockaddr_in*)&entry->addr)->sin_addr;
            h->is_ipv6 = false;
        } else if (entry->addr.ss_family == AF_INET6) {
            h->ip6 = ((struct sockaddr_in6*)&entry->addr)->sin6_addr;
            h->is_ipv6 = true;
        }
        strncpy(h->hostname, entry->hostname, sizeof(h->hostname)-1);

        // Allocate and copy ports for this host
        plan.ports[hidx] = malloc(sizeof(uint32_t) * g_config->num_ports);
        plan.target_ips[hidx] = malloc(INET_ADDRSTRLEN);
        if (!plan.ports[hidx] || !plan.target_ips[hidx]) {
            fprintf(stderr, "[-] Memory allocation failed for host %u\n", hidx);
            // Cleanup would be complex here, let main cleanup handle it
            break;
        }

        // Convert IP to string
        if (!h->is_ipv6) {
            inet_ntop(AF_INET, &h->ip4, plan.target_ips[hidx], INET_ADDRSTRLEN);
        } else {
            strncpy(plan.target_ips[hidx], "::1", INET_ADDRSTRLEN-1); // IPv6 placeholder
        }

        // Copy ports
        plan.port_counts[hidx] = g_config->num_ports;
        for (uint32_t p = 0; p < g_config->num_ports; p++) {
            plan.ports[hidx][p] = g_config->ports[p];
        }
    }

    /* Enqueue all tasks */
    if (scheduler_enqueue_plan(sched, &plan) < 0) {
        fprintf(stderr, "[-] Failed to enqueue scan plan\n");
        goto cleanup;
    }
    
    if (g_config->verbosity > 0) {
        printf("[+] Enqueued %u tasks for scanning\n", plan.num_total_tasks);
    }

    /* Main concurrent scanning loop */
    struct timeval concurrent_scan_start, concurrent_scan_end;
    gettimeofday(&concurrent_scan_start, NULL);

    uint32_t total_submitted = 0;
    uint32_t total_processed = 0;

    while (!scheduler_is_finished(sched)) {
        /* Submit new connections up to concurrency limit */
        while (total_submitted < plan.num_total_tasks) {
            task_t *task = scheduler_next_task(sched);
            if (!task) break; // No more tasks available or concurrency limit reached

            // Create connection for this task
            if (g_config->verbosity > 1) {
                printf("[*] Creating connection for host %u (%s) port %u\n", 
                       task->host_index, plan.target_ips[task->host_index], task->port);
            }
            connection_t *conn = connection_create(
                plan.target_ips[task->host_index],
                task->port,
                g_config->timeout_ms
            );
            if (!conn) {
                fprintf(stderr, "[-] Failed to create connection for %s:%u\n",
                        plan.target_ips[task->host_index], task->port);
                continue;
            }

            if (g_config->verbosity > 1) {
                printf("[*] Created connection for %s:%u\n", plan.target_ips[task->host_index], task->port);
            }

            // Set probe type and host context
            conn->probe_type = task->probe_type;
            conn->host_context = (void*)(uintptr_t)task->host_index;

            // Queue connection
            if (network_queue_connection(net_engine, conn) < 0) {
                fprintf(stderr, "[-] Failed to queue connection for %s:%u\n",
                        plan.target_ips[task->host_index], task->port);
                connection_free(conn);
                continue;
            }
            
            if (g_config->verbosity > 1) {
                printf("[*] Queued connection for %s:%u\n", plan.target_ips[task->host_index], task->port);
            }

            total_submitted++;
        }

        if (g_config->verbosity > 1) {
            printf("[*] Submitted %u/%u tasks\n", total_submitted, plan.num_total_tasks);
        }

        /* Process pending connections */
        int process_result = network_process_batch(net_engine, 10); // 10ms timeout
        if (process_result < 0) {
            fprintf(stderr, "[-] Network processing error: %d\n", process_result);
            break;
        }
        if (g_config->verbosity > 1 && process_result > 0) {
            printf("[*] Processed %d events\n", process_result);
        }

        /* Collect finished connections */
        connection_t **finished = NULL;
        uint32_t finished_count = 0;
        if (network_collect_finished(net_engine, &finished, &finished_count) == 0) {
            if (g_config->verbosity > 1 && finished_count > 0) {
                printf("[*] Collected %u finished connections\n", finished_count);
            }
            for (uint32_t i = 0; i < finished_count; i++) {
                connection_t *conn = finished[i];
                uint32_t host_idx = (uintptr_t)conn->host_context;
                host_info_t *h = &results[host_idx];

                // Map connection result to port state
                port_state_t state = PORT_UNKNOWN;
                conn_state_t conn_state = connection_get_state(conn);

                switch (conn_state) {
                    case CONN_STATE_OPEN:
                        state = PORT_OPEN;
                        break;
                    case CONN_STATE_CLOSED:
                    case CONN_STATE_RESET:
                        state = PORT_CLOSED;
                        break;
                    case CONN_STATE_TIMEOUT:
                        state = PORT_FILTERED;
                        break;
                    case CONN_STATE_ERROR:
                    default:
                        state = PORT_FILTERED;
                        break;
                }

                // Store result
                h->ports[h->num_ports].port = conn->port;
                h->ports[h->num_ports].state = state;
                h->num_ports++;

                if (state == PORT_OPEN) {
                    h->state = HOST_UP;
                    total_open++;
                    if (g_config->version_detection) {
                        port_info_t info;
                        memset(&info, 0, sizeof(info));
                        if (!h->is_ipv6) {
                            uint32_t ip = ntohl(h->ip4.s_addr);
                            detect_service(ip, conn->port, &info);
                        }
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

                // Mark task complete
                scheduler_mark_complete(sched, host_idx);
                total_processed++;

                // Free connection
                connection_free(conn);
            }
        }

        /* Rate limiting */
        if (g_config->max_rate > 0) {
            uint32_t delay_us = 1000000 / g_config->max_rate;
            usleep(delay_us);
        }
    }

    gettimeofday(&concurrent_scan_end, NULL);
    uint64_t scan_time_us = ((concurrent_scan_end.tv_sec - concurrent_scan_start.tv_sec) * 1000000ULL) +
                            (concurrent_scan_end.tv_usec - concurrent_scan_start.tv_usec);

    if (g_config->verbosity > 0) {
        printf("[+] Scan completed in %.3fs (%u ports, %.1f ports/sec)\n",
               scan_time_us / 1000000.0, total_processed,
               total_processed / (scan_time_us / 1000000.0));
    }

cleanup:
    /* Cleanup scan plan */
    if (plan.ports) {
        for (uint32_t i = 0; i < num_hosts; i++) {
            free(plan.ports[i]);
        }
        free(plan.ports);
    }
    if (plan.target_ips) {
        for (uint32_t i = 0; i < num_hosts; i++) {
            free(plan.target_ips[i]);
        }
        free(plan.target_ips);
    }
    free(plan.port_counts);

    /* Cleanup components */
    scheduler_free(sched);
    network_engine_cleanup(net_engine);
    
    /* Print results - show host discovery state */
    for (uint32_t i = 0; i < total_hosts; i++) {
        host_info_t *h = &results[i];
        char ip_str[INET_ADDRSTRLEN];
        
        if (!h->is_ipv6) {
            inet_ntop(AF_INET, &h->ip4, ip_str, sizeof(ip_str));
        } else {
            strncpy(ip_str, "::1", sizeof(ip_str) - 1);  /* Placeholder for IPv6 */
        }
        
        if (g_config->verbosity > 1) {
            printf("[*] Host %s: state=%s\n", ip_str, h->state == HOST_UP ? "UP" : "DOWN");
        }
        
        if (h->state == HOST_UP) {
            printf("Nmap scan report for %s\n", ip_str);
            if (h->hostname[0] && strcmp(h->hostname, ip_str) != 0) {
                printf("Hostname: %s\n", h->hostname);
            }
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
    if (g_engine) {
        network_engine_cleanup(g_engine);
        g_engine = NULL;
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
