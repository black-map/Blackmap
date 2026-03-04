#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "blackmap.h"

/* Target parsing - CIDR, ranges, single hosts, DNS names */

static uint32_t resolve_hostname(const char *hostname) {
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname, NULL, &hints, &result) == 0) {
        uint32_t ip = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
        freeaddrinfo(result);
        return ip;
    }
    return 0;
}

int parse_ipv4_target(const char *target, uint32_t *ip_start, uint32_t *ip_end) {
    if (!target || !ip_start || !ip_end) return -1;
    
    char buf[256];
    strncpy(buf, target, sizeof(buf) - 1);
    
    /* Check for CIDR notation (192.168.1.0/24) */
    char *slash = strchr(buf, '/');
    if (slash) {
        *slash = '\0';
        int prefix = atoi(slash + 1);
        
        struct in_addr addr;
        if (inet_aton(buf, &addr) == 0) {
            fprintf(stderr, "[-] Invalid CIDR: %s\n", target);
            return -1;
        }
        
        uint32_t network = ntohl(addr.s_addr);
        uint32_t mask = (0xFFFFFFFFu << (32 - prefix)) & 0xFFFFFFFFu;
        
        *ip_start = network & mask;
        *ip_end = network | ~mask;
        
        if (g_config->debug) {
            printf("[DEBUG] CIDR %s -> %u.%u.%u.%u - %u.%u.%u.%u\n",
                   target,
                   (*ip_start >> 24) & 0xFF, (*ip_start >> 16) & 0xFF,
                   (*ip_start >> 8) & 0xFF, *ip_start & 0xFF,
                   (*ip_end >> 24) & 0xFF, (*ip_end >> 16) & 0xFF,
                   (*ip_end >> 8) & 0xFF, *ip_end & 0xFF);
        }
        return 0;
    }
    
    /* Check for range notation (192.168.1.1-254) */
    char *dash = strrchr(buf, '-');
    if (dash && dash != buf && !strchr(buf, '.')) {  /* Avoid matching negative numbers */
        *dash = '\0';
        int start_octet = atoi(buf);
        int end_octet = atoi(dash + 1);
        
        if (start_octet >= 0 && start_octet <= 255 && end_octet >= 0 && end_octet <= 255) {
            /* Parse base IP */
            char base_ip[256];
            strncpy(base_ip, buf, sizeof(base_ip) - 1);
            
            /* Remove the last octet */
            char *last_dot = strrchr(base_ip, '.');
            if (last_dot) {
                *last_dot = '\0';
                
                struct in_addr addr;
                char full_ip_start[256], full_ip_end[256];
                
                snprintf(full_ip_start, sizeof(full_ip_start), "%s.%d", base_ip, start_octet);
                snprintf(full_ip_end, sizeof(full_ip_end), "%s.%d", base_ip, end_octet);
                
                if (inet_aton(full_ip_start, &addr) == 0) {
                    return -1;
                }
                *ip_start = ntohl(addr.s_addr);
                
                if (inet_aton(full_ip_end, &addr) == 0) {
                    return -1;
                }
                *ip_end = ntohl(addr.s_addr);
                
                return 0;
            }
        }
    }
    
    /* Single IP address or hostname */
    struct in_addr addr;
    if (inet_aton(buf, &addr) == 0) {
        /* Try DNS lookup */
        uint32_t resolved_ip = resolve_hostname(buf);
        if (resolved_ip == 0) {
            fprintf(stderr, "[-] Invalid IP or hostname: %s\n", target);
            return -1;
        }
        *ip_start = ntohl(resolved_ip);
        *ip_end = ntohl(resolved_ip);
        
        if (g_config->verbosity > 0 || g_config->debug) {
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &resolved_ip, ip_str, INET_ADDRSTRLEN);
            printf("[+] Resolved %s -> %s\n", target, ip_str);
        }
        return 0;
    }
    
    uint32_t ip = ntohl(addr.s_addr);
    *ip_start = ip;
    *ip_end = ip;
    
    if (g_config->debug) {
        printf("[DEBUG] Single IP: %u.%u.%u.%u\n",
               (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
               (ip >> 8) & 0xFF, ip & 0xFF);
    }
    
    return 0;
}

int count_targets(const char *target_str) {
    if (!target_str) return 0;
    
    uint32_t total = 0;
    char buf[4096];
    strncpy(buf, target_str, sizeof(buf) - 1);
    
    char *token = strtok(buf, ",");
    while (token) {
        uint32_t ip_start, ip_end;
        
        if (parse_ipv4_target(token, &ip_start, &ip_end) == 0) {
            total += (ip_end - ip_start + 1);
            
            if (total > MAX_TARGETS) {
                fprintf(stderr, "[-] Target count exceeds maximum (%u)\n", MAX_TARGETS);
                return -1;
            }
        }
        
        token = strtok(NULL, ",");
    }
    
    return total;
}
