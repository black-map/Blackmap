#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "proxy.h"
#include "blackmap.h"

bool detect_proxy_active(void) {
    char *ld_preload = getenv("LD_PRELOAD");
    if (ld_preload) {
        if (strstr(ld_preload, "proxychains") || strstr(ld_preload, "torsocks")) {
            return true;
        }
    }
    return false;
}

void enforce_proxy_mode(void) {
    if (g_config->proxy_enforced && !detect_proxy_active()) {
        fprintf(stderr, "Error: --proxy-enforced specified but no proxy detected (LD_PRELOAD not set to proxychains/torsocks)\n");
        exit(EXIT_FAILURE);
    }
    if (detect_proxy_active()) {
        // Force TCP connect scan
        g_config->scan_type = SCAN_TYPE_CONNECT;
        // Disable raw sockets
        disable_raw_sockets_for_proxy();
        // Disable OS detection
        disable_os_detection_for_proxy();
        if (g_config->verbosity > 0) {
            fprintf(stderr, "[*] Proxy detected, forcing TCP connect scan and disabling raw sockets/OS detection\n");
        }
    }
}

void disable_raw_sockets_for_proxy(void) {
    // Raw sockets don't work with proxies, so ensure we don't use them
    // This is already handled by forcing SCAN_TYPE_CONNECT
}

void disable_os_detection_for_proxy(void) {
    if (g_config->os_detection) {
        g_config->os_detection = false;
        if (g_config->verbosity > 0) {
            fprintf(stderr, "[*] OS detection disabled due to proxy transport limitations\n");
        }
    }
}