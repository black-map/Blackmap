#include "blackmap3/discovery.h"
#include "blackmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>

/* Default probe ports for TCP discovery */
static const uint16_t DEFAULT_PROBE_PORTS[] = {
    80, 443, 22, 3389, 445, 139, 21, 25, 53, 
    88, 389, 636, 3306, 5432, 5985, 27017, 50500
};
#define DEFAULT_PROBE_PORTS_COUNT (sizeof(DEFAULT_PROBE_PORTS) / sizeof(DEFAULT_PROBE_PORTS[0]))

discovery_config_t* discovery_config_create(void) {
    discovery_config_t *config = calloc(1, sizeof(discovery_config_t));
    if (!config) return NULL;
    
    config->method = DISCOVERY_METHOD_COMBINED;
    config->timeout_ms = 3000;
    config->max_retries = 2;
    config->skip_discovery = false;
    config->verbose = false;
    
    /* Allocate and copy default probe ports */
    config->probe_ports = malloc(sizeof(DEFAULT_PROBE_PORTS));
    if (!config->probe_ports) {
        free(config);
        return NULL;
    }
    memcpy(config->probe_ports, DEFAULT_PROBE_PORTS, sizeof(DEFAULT_PROBE_PORTS));
    config->probe_port_count = DEFAULT_PROBE_PORTS_COUNT;
    
    return config;
}

void discovery_config_free(discovery_config_t *config) {
    if (!config) return;
    if (config->probe_ports) free(config->probe_ports);
    free(config);
}

/**
 * Set socket to non-blocking mode
 */
static int set_socket_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int discovery_tcp_connect_ping(struct sockaddr_storage *addr, 
                              uint16_t port,
                              uint32_t timeout_ms) {
    if (!addr) return -1;
    
    int sock = socket(addr->ss_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) return -1;
    
    /* Set socket to non-blocking for timeout control */
    if (set_socket_nonblocking(sock) == -1) {
        close(sock);
        return -1;
    }
    
    /* Get address pointer and set port */
    struct sockaddr *sa = (struct sockaddr *)addr;
    socklen_t addr_len;
    
    if (addr->ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        sin->sin_port = htons(port);
        addr_len = sizeof(struct sockaddr_in);
    } else if (addr->ss_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
        sin6->sin6_port = htons(port);
        addr_len = sizeof(struct sockaddr_in6);
    } else {
        close(sock);
        return -1;
    }
    
    /* Attempt non-blocking connect */
    int result = -1;
    if (connect(sock, sa, addr_len) == 0) {
        /* Immediate success (shouldn't happen with non-blocking) */
        result = 0;
    } else if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
        /* Connection in progress - wait with select */
        fd_set writefds;
        struct timeval tv;
        
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);
        
        int select_result = select(sock + 1, NULL, &writefds, NULL, &tv);
        if (select_result > 0 && FD_ISSET(sock, &writefds)) {
            /* Check actual connection status */
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) != -1 && error == 0) {
                result = 0;  /* Success! */
            }
        }
    }
    
    close(sock);
    return result;
}

/**
 * Simple ICMP checksum calculation
 */
static uint16_t icmp_checksum(void *buf, int len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)buf;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(uint8_t *)ptr;
    }
    
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

int discovery_icmp_echo_ping(struct sockaddr_storage *addr,
                            uint32_t timeout_ms) {
    if (!addr || addr->ss_family != AF_INET) return -1;
    if (geteuid() != 0) return -1;  /* Requires root */
    
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock == -1) return -1;
    
    if (set_socket_nonblocking(sock) == -1) {
        close(sock);
        return -1;
    }
    
    /* Build ICMP echo request - use simple byte array */
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    
    buf[0] = ICMP_ECHO;           /* Type */
    buf[1] = 0;                   /* Code */
    /* Checksum at buf[2-3] */
    *(uint16_t *)(buf + 4) = htons(getpid());      /* ID */
    *(uint16_t *)(buf + 6) = htons(1);             /* Sequence */
    
    /* Calculate ICMP checksum */
    uint16_t *icmp_cksum_ptr = (uint16_t *)(buf + 2);
    *icmp_cksum_ptr = icmp_checksum(buf, sizeof(buf));
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    
    /* Send ICMP echo */
    if (sendto(sock, buf, sizeof(buf), 0, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
        close(sock);
        return -1;
    }
    
    /* Wait for reply */
    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    
    int result = -1;
    int select_result = select(sock + 1, &readfds, NULL, NULL, &tv);
    if (select_result > 0) {
        struct sockaddr_in src;
        socklen_t src_len = sizeof(src);
        uint8_t reply_buf[256];
        
        ssize_t len = recvfrom(sock, reply_buf, sizeof(reply_buf), 0,
                              (struct sockaddr *)&src, &src_len);
        if (len > 0) {
            /* Parse IP header and check ICMP reply */
            struct ip *ip_hdr = (struct ip *)reply_buf;
            int ip_hlen = ip_hdr->ip_hl * 4;
            
            if (len >= ip_hlen + 8) {
                uint8_t icmp_type = reply_buf[ip_hlen];
                if (icmp_type == ICMP_ECHOREPLY) {
                    result = 0;  /* Success! */
                }
            }
        }
    }
    
    close(sock);
    return result;
}

int discovery_tcp_syn_ping(struct sockaddr_storage *addr,
                          uint16_t port,
                          uint32_t timeout_ms) {
    /* SYN ping requires root and raw sockets - complex implementation
       For now, fall back to TCP CONNECT ping */
    (void)timeout_ms;
    return discovery_tcp_connect_ping(addr, port, timeout_ms);
}

int discovery_tcp_ack_ping(struct sockaddr_storage *addr,
                          uint16_t port,
                          uint32_t timeout_ms) {
    /* ACK ping requires root and raw sockets - complex implementation
       For now, fall back to TCP CONNECT ping */
    (void)timeout_ms;
    return discovery_tcp_connect_ping(addr, port, timeout_ms);
}

int discovery_udp_probe(struct sockaddr_storage *addr,
                       uint16_t port,
                       uint32_t timeout_ms) {
    if (!addr) return -1;
    
    int sock = socket(addr->ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == -1) return -1;
    
    if (set_socket_nonblocking(sock) == -1) {
        close(sock);
        return -1;
    }
    
    struct sockaddr *sa = (struct sockaddr *)addr;
    socklen_t addr_len_udp;
    
    /* Set port */
    if (addr->ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        sin->sin_port = htons(port);
        addr_len_udp = sizeof(struct sockaddr_in);
    } else if (addr->ss_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
        sin6->sin6_port = htons(port);
        addr_len_udp = sizeof(struct sockaddr_in6);
    } else {
        close(sock);
        return -1;
    }
    
    /* Send UDP probe packet (empty) */
    if (sendto(sock, "", 0, 0, sa, addr_len_udp) < 0) {
        close(sock);
        return -1;  /* Can't send = host down */
    }
    
    /* Wait for response with select */
    fd_set readfds;
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    
    int result = -1;
    int select_result = select(sock + 1, &readfds, NULL, NULL, &tv);
    if (select_result > 0) {
        uint8_t buf[256];
        struct sockaddr_storage src;
        socklen_t src_len = sizeof(src);
        
        ssize_t len = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
                              (struct sockaddr *)&src, &src_len);
        if (len > 0 || (len < 0 && errno == ICMP_UNREACH)) {
            /* Got something (response or ICMP error) = host is up */
            result = 0;
        }
    }
    
    close(sock);
    return result;
}

int discovery_probe_host(const discovery_config_t *config,
                        struct sockaddr_storage *target,
                        discovery_result_t *result) {
    if (!config || !target || !result) return -1;
    
    memset(result, 0, sizeof(discovery_result_t));
    result->addr = *target;
    if (target->ss_family == AF_INET) {
        result->addr_len = sizeof(struct sockaddr_in);
    } else if (target->ss_family == AF_INET6) {
        result->addr_len = sizeof(struct sockaddr_in6);
    } else {
        result->addr_len = 0;
    }
    result->family = target->ss_family;
    
    /* Convert address to string for logging */
    if (target->ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)target;
        inet_ntop(AF_INET, &sin->sin_addr, result->addr_str, INET6_ADDRSTRLEN);
    } else if (target->ss_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)target;
        inet_ntop(AF_INET6, &sin6->sin6_addr, result->addr_str, INET6_ADDRSTRLEN);
    }
    
    /* Skip discovery if -Pn specified */
    if (config->skip_discovery) {
        result->is_up = true;
        result->probe_method_used = DISCOVERY_METHOD_NONE;
        return 0;
    }
    
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);
    
    int root = (geteuid() == 0);
    
    /* Try multiple discovery methods in order of preference */
    if (root && discovery_icmp_echo_ping(target, config->timeout_ms) == 0) {
        result->is_up = true;
        result->probe_method_used = DISCOVERY_PROBE_ICMP;
        if (config->verbose) {
            printf("[*] Host %s is up (ICMP echo reply)\n", result->addr_str);
        }
        goto done;
    }
    
    /* Try multiple TCP ports for CONNECT ping */
    for (uint16_t i = 0; i < config->probe_port_count; i++) {
        if (discovery_tcp_connect_ping(target, config->probe_ports[i], config->timeout_ms) == 0) {
            result->is_up = true;
            result->probe_method_used = DISCOVERY_PROBE_TCP_CONNECT;
            if (config->verbose) {
                printf("[*] Host %s is up (TCP port %u connection successful)\n", 
                       result->addr_str, config->probe_ports[i]);
            }
            goto done;
        }
    }
    
    /* If still not found and root, try TCP SYN (falls back to CONNECT) */
    if (root && discovery_tcp_syn_ping(target, 80, config->timeout_ms) == 0) {
        result->is_up = true;
        result->probe_method_used = DISCOVERY_PROBE_TCP_SYN;
        if (config->verbose) {
            printf("[*] Host %s is up (TCP SYN ping)\n", result->addr_str);
        }
        goto done;
    }
    
    /* If still not found and root, try TCP ACK */
    if (root && discovery_tcp_ack_ping(target, 80, config->timeout_ms) == 0) {
        result->is_up = true;
        result->probe_method_used = DISCOVERY_PROBE_TCP_ACK;
        if (config->verbose) {
            printf("[*] Host %s is up (TCP ACK ping)\n", result->addr_str);
        }
        goto done;
    }
    
    /* No response from any probe */
    result->is_up = false;
    result->probe_method_used = DISCOVERY_METHOD_NONE;
    if (config->verbose) {
        printf("[*] Host %s appears down (no response to probes)\n", result->addr_str);
    }

done:
    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);
    result->rtt_ms = (tv_end.tv_sec - tv_start.tv_sec) * 1000 +
                    (tv_end.tv_usec - tv_start.tv_usec) / 1000;
    
    return 0;
}

int discovery_probe_hosts(const discovery_config_t *config,
                         struct sockaddr_storage *targets,
                         uint32_t target_count,
                         discovery_result_t *results,
                         discovery_stats_t *stats) {
    if (!config || !targets || !results) return -1;
    
    if (stats) memset(stats, 0, sizeof(discovery_stats_t));
    
    struct timeval tv_start;
    gettimeofday(&tv_start, NULL);
    
    uint32_t hosts_up = 0;
    
    for (uint32_t i = 0; i < target_count; i++) {
        if (discovery_probe_host(config, &targets[i], &results[i]) == 0) {
            if (results[i].is_up) {
                hosts_up++;
            }
            if (stats) {
                stats->total_probes_sent++;
                if (results[i].is_up) {
                    stats->successful_probes++;
                } else {
                    stats->failed_probes++;
                }
            }
        } else {
            if (stats) stats->failed_probes++;
        }
    }
    
    struct timeval tv_end;
    gettimeofday(&tv_end, NULL);
    
    if (stats) {
        stats->hosts_discovered_up = hosts_up;
        stats->duration_ms = (tv_end.tv_sec - tv_start.tv_sec) * 1000 +
                            (tv_end.tv_usec - tv_start.tv_usec) / 1000;
    }
    
    return hosts_up;
}
