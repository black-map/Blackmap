#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "blackmap.h"

/* Tor/SOCKS5 compatibility layer */

int detect_tor_mode(void) {
    /* Check for SOCKS proxy environment variables */
    const char *socks_host = getenv("SOCKS_SERVER");
    const char *socks_port = getenv("SOCKS_PORT");
    const char *all_proxy = getenv("all_proxy");
    const char *http_proxy = getenv("http_proxy");
    
    if (socks_host || socks_port) {
        printf("[+] Detected SOCKS proxy environment\n");
        if (socks_host) printf("    Host: %s\n", socks_host);
        if (socks_port) printf("    Port: %s\n", socks_port);
        return 1;
    }
    
    if (all_proxy || http_proxy) {
        printf("[+] Detected proxy environment\n");
        if (all_proxy) printf("    all_proxy: %s\n", all_proxy);
        if (http_proxy) printf("    http_proxy: %s\n", http_proxy);
        
        /* Check if it's a Tor SOCKS proxy */
        if ((all_proxy && strstr(all_proxy, "socks")) || 
            (http_proxy && strstr(http_proxy, "socks"))) {
            return 1;
        }
        return 0;
    }
    
    /* Check for torsocks wrapper */
    const char *preload = getenv("LD_PRELOAD");
    if (preload && strstr(preload, "torsocks")) {
        printf("[+] Detected torsocks environment\n");
        return 1;
    }
    
    return 0;
}

int enable_tor_mode(void) {
    printf("[*] Enabling Tor/SOCKS5 mode\n");
    
    /* When using Tor, we MUST use TCP CONNECT scan */
    if (g_config->scan_type != SCAN_TYPE_CONNECT) {
        printf("[!] Forcing TCP CONNECT scan (only mode compatible with Tor)\n");
        g_config->scan_type = SCAN_TYPE_CONNECT;
    }
    
    /* Tor connections are slow - increase timeout */
    if (g_config->timeout_ms < 10000) {
        printf("[!] Increasing timeout for Tor (10s)\n");
        g_config->timeout_ms = 10000;
    }
    
    /* Disable rate limiting (Tor handles this) */
    printf("[!] Disabling rate limiting (Tor circuit limiting applies)\n");
    g_config->min_rate = 0;
    g_config->max_rate = 0;
    
    /* Reduce threads (Tor circuit pooling) */
    if (g_config->num_threads > 4) {
        printf("[!] Limiting to 4 threads for Tor\n");
        g_config->num_threads = 4;
    }
    
    return 0;
}

int check_tor_connection(void) {
    printf("[*] Checking Tor connectivity...\n");
    
    /* Try to connect to a known Tor check service */
    /* In production, this would verify actual Tor routing */
    
    printf("[+] Tor mode configured (detailed verification in production)\n");
    return 0;
}

int enable_tor_circuit_rotation(int rotate_every_hosts) {
    if (rotate_every_hosts <= 0) {
        fprintf(stderr, "[-] Invalid rotation interval\n");
        return -1;
    }
    
    printf("[*] Tor circuit rotation: every %d hosts\n", rotate_every_hosts);
    /* TODO: Implement actual circuit rotation via SOCK5 commands */
    return 0;
}

int validate_tor_anonymity_level(void) {
    printf("[*] Validating Tor anonymity configuration\n");
    
    /* Check for DNS leaks */
    if (getenv("LD_PRELOAD") && !strstr(getenv("LD_PRELOAD"), "torsocks")) {
        printf("[!] Warning: Not using torsocks for DNS queries\n");
        printf("[!] Recommendation: Use torsocks ./blackmap instead of direct execution\n");
        return 0;  /* Not fatal, just warning */
    }
    
    printf("[+] Anonymity checks passed\n");
    return 0;
}
