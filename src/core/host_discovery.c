#include "blackmap.h"
#include "blackmap3/host_discovery.h"
#include "blackmap3/dns.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>

/* simple helper used by ICMP code */
static uint16_t icmp_checksum(void *buf, int len) {
    uint32_t sum = 0;
    uint16_t *ptr = buf;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(uint8_t*)ptr;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

/* Utility that sets a socket non-blocking */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* send one ICMP echo request for a given host; id/seq allow matching replies */
static int send_icmp_echo(int sock, const host_entry_t *host, uint16_t id, uint16_t seq) {
    struct icmp icmp_hdr;
    memset(&icmp_hdr, 0, sizeof(icmp_hdr));
    icmp_hdr.icmp_type = ICMP_ECHO;
    icmp_hdr.icmp_code = 0;
    icmp_hdr.icmp_id = htons(id);
    icmp_hdr.icmp_seq = htons(seq);
    icmp_hdr.icmp_cksum = 0;
    icmp_hdr.icmp_cksum = icmp_checksum(&icmp_hdr, sizeof(icmp_hdr));

    struct sockaddr *dst;
    socklen_t dstlen;

    if (host->addr.ss_family == AF_INET) {
        dst = (struct sockaddr*)&host->addr;
        dstlen = sizeof(struct sockaddr_in);
    } else {
        /* for now only IPv4 handled */
        return -1;
    }

    ssize_t rv = sendto(sock, &icmp_hdr, sizeof(icmp_hdr), 0, dst, dstlen);
    if (rv < 0) {
        return -1;
    }
    return 0;
}

/* receive from ICMP socket until timeout_ms expires or until we have collected
 * responses matching hosts.  This function will update hosts[i].state = HOST_UP
 * when a matching echo reply arrives.  The mapping between ip and host index is
 * done by comparing source address string.
 */
static void process_icmp_responses(int sock, host_entry_t *hosts, uint32_t count, uint32_t timeout_ms) {
    struct epoll_event ev;
    int epfd = epoll_create1(0);
    if (epfd < 0) return;

    ev.events = EPOLLIN;
    ev.data.fd = sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);

    struct timeval start;
    gettimeofday(&start, NULL);

    /* use a temporary buffer to read replies */
    uint8_t buf[1500];

    while (1) {
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                       (now.tv_usec - start.tv_usec) / 1000;
        if (elapsed >= timeout_ms) break;

        int remaining = timeout_ms - elapsed;
        int nfds = epoll_wait(epfd, &ev, 1, remaining);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            break;
        } else if (nfds == 0) {
            break;
        }

        if (ev.events & EPOLLIN) {
            struct sockaddr_storage src;
            socklen_t srclen = sizeof(src);
            ssize_t len = recvfrom(sock, buf, sizeof(buf), 0,
                                   (struct sockaddr*)&src, &srclen);
            if (len <= 0) continue;

            /* parse IP header to skip it */
            struct ip *ip_hdr = (struct ip*)buf;
            int ip_hdr_len = ip_hdr->ip_hl * 4;
            if (len < ip_hdr_len + sizeof(struct icmp)) continue;
            struct icmp *icmp_hdr = (struct icmp*)(buf + ip_hdr_len);
            if (icmp_hdr->icmp_type != ICMP_ECHOREPLY) continue;

            /* identify which host this came from */
            char src_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &((struct sockaddr_in*)&src)->sin_addr,
                      src_ip, sizeof(src_ip));

            for (uint32_t i = 0; i < count; i++) {
                if (strcmp(hosts[i].addr_str, src_ip) == 0) {
                    hosts[i].state = HOST_UP;
                    if (g_config && g_config->verbosity > 1) {
                        printf("[DEBUG] ICMP reply from %s\n", src_ip);
                    }
                }
            }
        }
    }

    close(epfd);
}

/* simple tcp connect ping wrapper using existing connect-scan function */
static int tcp_connect_ping(const host_entry_t *host, uint16_t port, int timeout_ms) {
    /* only IPv4 supported for now */
    if (host->addr.ss_family != AF_INET) return -1;
    uint32_t ip = ((struct sockaddr_in*)&host->addr)->sin_addr.s_addr;
    int state = tcp_connect_scan(ntohl(ip), port, timeout_ms);
    if (state == PORT_OPEN || state == PORT_CLOSED) {
        return 0; /* host is up if any reply received */
    }
    return -1;
}

/* stub functions for tcp_syn and tcp_ack ping, left as future extension
 * Real implementation would build raw TCP packets and listen for replies.
 */
static int tcp_syn_ping(const host_entry_t *host, int timeout_ms) {
    /* TODO: craft raw SYN packet and wait for SYN/ACK or RST reply */
    (void)host;
    (void)timeout_ms;
    return -1;
}
static int tcp_ack_ping(const host_entry_t *host, int timeout_ms) {
    /* TODO: craft raw ACK packet and wait for RST reply
       used when scanning through stateful firewalls */
    (void)host;
    (void)timeout_ms;
    return -1;
}

/* build_host_list implementation */
int build_host_list(const char *targets, host_entry_t **out_hosts, uint32_t *out_count) {
    if (!targets || !out_hosts || !out_count) return -1;

    char *copy = strdup(targets);
    if (!copy) return -1;

    host_entry_t *list = NULL;
    uint32_t count = 0;

    if (g_config && g_config->verbosity > 1) {
        printf("[DEBUG] Resolving target(s): %s\n", targets);
    }

    char *token = strtok(copy, ",");
    while (token) {
        /* Skip leading whitespace */
        while (*token && isspace((unsigned char)*token)) token++;
        if (*token == '\0') {
            token = strtok(NULL, ",");
            continue;
        }

        /* attempt CIDR/range parsing using existing helper */
        uint32_t ip_start, ip_end;
        if (parse_ipv4_target(token, &ip_start, &ip_end) == 0) {
            /* cidr or range or single ip; parse_ipv4_target already logged errors */
            for (uint32_t ip = ip_start; ip <= ip_end; ip++) {
                host_entry_t h;
                memset(&h, 0, sizeof(h));
                h.addr.ss_family = AF_INET;
                ((struct sockaddr_in*)&h.addr)->sin_family = AF_INET;
                ((struct sockaddr_in*)&h.addr)->sin_addr.s_addr = htonl(ip);
                h.addr_len = sizeof(struct sockaddr_in);
                h.is_ipv6 = false;
                h.state = HOST_UNKNOWN;
                inet_ntop(AF_INET, &((struct sockaddr_in*)&h.addr)->sin_addr,
                          h.addr_str, sizeof(h.addr_str));
                strncpy(h.hostname, token, sizeof(h.hostname)-1);

                if (g_config && g_config->verbosity > 1) {
                    printf("[DEBUG] Added IP: %s\n", h.addr_str);
                }

                list = realloc(list, sizeof(*list) * (count + 1));
                if (!list) {
                    free(copy);
                    return -1;
                }
                list[count++] = h;
            }
        } else {
            /* treat as hostname and resolve */
            dns_addr_t *addrs = NULL;
            size_t naddrs = 0;
            
            if (g_config && g_config->verbosity > 1) {
                printf("[DEBUG] Attempting DNS resolution for: %s\n", token);
            }
            
            if (resolve_hostname(token, &addrs, &naddrs) == 0 && naddrs > 0) {
                if (g_config && g_config->verbosity > 1) {
                    printf("[DEBUG] DNS resolved %s to %zu address(es)\n", token, naddrs);
                }
                
                for (size_t j = 0; j < naddrs; j++) {
                    host_entry_t h;
                    memset(&h, 0, sizeof(h));
                    h.addr = addrs[j].addr;
                    h.addr_len = addrs[j].addr_len;
                    h.is_ipv6 = (addrs[j].addr.ss_family == AF_INET6);
                    h.state = HOST_UNKNOWN;
                    strncpy(h.addr_str, addrs[j].addr_str, sizeof(h.addr_str)-1);
                    strncpy(h.hostname, token, sizeof(h.hostname)-1);

                    if (g_config && g_config->verbosity > 1) {
                        printf("[DEBUG]   -> %s\n", h.addr_str);
                    }

                    list = realloc(list, sizeof(*list) * (count + 1));
                    if (!list) {
                        free(addrs);
                        free(copy);
                        return -1;
                    }
                    list[count++] = h;
                }
                free(addrs);
            } else {
                /* DNS resolution failed
                
                   DO NOT add a HOST_DOWN entry with no address!
                   Instead, print an error and skip this target.
                   The user should fix their target specification.
                */
                if (g_config && g_config->verbosity > 0) {
                    fprintf(stderr, "[-] Failed to resolve hostname: %s\n", token);
                }
                if (g_config && g_config->verbosity > 1) {
                    fprintf(stderr, "[DEBUG] Check DNS configuration or enter valid IP/hostname\n");
                }
            }
        }

        token = strtok(NULL, ",");
    }

    free(copy);
    
    if (count == 0) {
        if (g_config && g_config->verbosity > 0) {
            fprintf(stderr, "[-] No valid targets resolved\n");
        }
        *out_hosts = NULL;
        *out_count = 0;
        return -1;  /* Indicate failure if no targets */
    }
    
    *out_hosts = list;
    *out_count = count;
    return 0;
}

void host_list_free(host_entry_t *hosts, uint32_t count) {
    (void)count;
    free(hosts);
}

int host_discovery_run(host_entry_t *hosts, uint32_t count) {
    if (!hosts) return -1;

    if (g_config->skip_ping) {
        if (g_config->verbosity > 0) {
            printf("[+] -Pn specified, skipping host discovery\n");
        }
        for (uint32_t i = 0; i < count; i++) {
            hosts[i].state = HOST_UP;
        }
        return count;
    }

    if (g_config->verbosity > 0) {
        printf("[*] Starting host discovery (%u target%s)\n", count, count==1?"":"s");
    }

    int icmp_sock = -1;
    bool have_icmp = false;
    if (geteuid() == 0) {
        icmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (icmp_sock >= 0) {
            set_nonblocking(icmp_sock);
            have_icmp = true;
        }
    }

    /* send ICMP echoes first when possible */
    if (have_icmp) {
        uint16_t pid = (uint16_t)getpid();
        for (uint32_t i = 0; i < count; i++) {
            if (hosts[i].addr.ss_family == AF_INET) {
                send_icmp_echo(icmp_sock, &hosts[i], pid, i);
            }
        }
        process_icmp_responses(icmp_sock, hosts, count, g_config->timeout_ms);
    }

    int hosts_up = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (hosts[i].state == HOST_UP) {
            hosts_up++;
            continue;
        }
        /* try TCP connect to common port as fallback (non-root) */
        if (tcp_connect_ping(&hosts[i], 80, g_config->timeout_ms) == 0) {
            hosts[i].state = HOST_UP;
            hosts_up++;
            if (g_config->verbosity > 1) {
                printf("[DEBUG] TCP connect ping succeeded for %s\n", hosts[i].addr_str);
            }
            continue;
        }
        /* if still down and root, try SYN and ACK pings (not implemented) */
        if (geteuid() == 0) {
            if (tcp_syn_ping(&hosts[i], g_config->timeout_ms) == 0) {
                hosts[i].state = HOST_UP;
                hosts_up++;
                continue;
            }
            if (tcp_ack_ping(&hosts[i], g_config->timeout_ms) == 0) {
                hosts[i].state = HOST_UP;
                hosts_up++;
                continue;
            }
        }
    }

    if (have_icmp) close(icmp_sock);

    if (g_config->verbosity > 0) {
        printf("[*] Host discovery complete: %d host(s) up\n", hosts_up);
    }
    return hosts_up;
}
