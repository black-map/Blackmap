#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "blackmap.h"

/* Port parsing - ranges, lists, services */

static struct {
    const char *name;
    uint16_t port;
} common_services[] = {
    {"ssh", 22},
    {"telnet", 23},
    {"smtp", 25},
    {"dns", 53},
    {"http", 80},
    {"pop3", 110},
    {"imap", 143},
    {"https", 443},
    {"mysql", 3306},
    {"rdp", 3389},
    {NULL, 0}
};

uint16_t service_to_port(const char *service) {
    if (!service) return 0;
    
    for (int i = 0; common_services[i].name; i++) {
        if (strcmp(common_services[i].name, service) == 0) {
            return common_services[i].port;
        }
    }
    return 0;
}

int parse_ports(const char *port_spec) {
    if (!port_spec) {
        /* Default: top 1000 ports */
        g_config->num_ports = 0;
        g_config->ports = malloc(sizeof(uint16_t) * 1000);
        if (!g_config->ports) {
            perror("malloc");
            return -1;
        }
        
        /* Add common ports */
        int idx = 0;
        uint16_t common[] = {22, 25, 80, 110, 143, 443, 465, 587, 993, 995,
                             1080, 3306, 3389, 5432, 5900, 8080, 8443};
        for (uint32_t i = 0; i < sizeof(common) / sizeof(common[0]); i++) {
            g_config->ports[idx++] = common[i];
        }
        g_config->num_ports = idx;
        return 0;
    }
    
    char buf[4096];
    strncpy(buf, port_spec, sizeof(buf) - 1);
    
    /* Count ports first */
    int count = 0;
    char tmp[4096];
    strncpy(tmp, buf, sizeof(tmp) - 1);
    
    char *token = strtok(tmp, ",");
    while (token) {
        count++;
        token = strtok(NULL, ",");
    }
    
    if (count == 0) {
        fprintf(stderr, "[-] Invalid port specification\n");
        return -1;
    }
    
    g_config->ports = malloc(sizeof(uint16_t) * count);
    if (!g_config->ports) {
        perror("malloc");
        return -1;
    }
    
    g_config->num_ports = 0;
    token = strtok(buf, ",");
    
    while (token) {
        /* Skip whitespace */
        while (isspace(*token)) token++;
        
        /* Check for range (22-25) */
        char *dash = strchr(token, '-');
        if (dash && isdigit(*(dash - 1)) && isdigit(*(dash + 1))) {
            *dash = '\0';
            int start = atoi(token);
            int end = atoi(dash + 1);
            
            if (start < 1 || start > 65535 || end < 1 || end > 65535 || end < start) {
                fprintf(stderr, "[-] Invalid port range: %s-%s\n", token, dash + 1);
                return -1;
            }
            
            for (int i = start; i <= end; i++) {
                g_config->ports[g_config->num_ports++] = i;
            }
        }
        /* Check if it's a service name */
        else if (isalpha(*token)) {
            uint16_t port = service_to_port(token);
            if (port == 0) {
                fprintf(stderr, "[-] Unknown service: %s\n", token);
                return -1;
            }
            g_config->ports[g_config->num_ports++] = port;
        }
        /* Single port */
        else if (isdigit(*token)) {
            int port = atoi(token);
            if (port < 1 || port > 65535) {
                fprintf(stderr, "[-] Invalid port: %d\n", port);
                return -1;
            }
            g_config->ports[g_config->num_ports++] = port;
        }
        
        token = strtok(NULL, ",");
    }
    
    if (g_config->debug) {
        printf("[DEBUG] Parsed %u ports\n", g_config->num_ports);
    }
    
    return 0;
}
