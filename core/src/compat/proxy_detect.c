#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "blackmap.h"

/* Proxy Detection and Compatibility Layer */

int detect_proxy_env(void) {
    const char *preload = getenv("LD_PRELOAD");
    
    if (!preload) {
        return 0;  /* No proxy */
    }
    
    if (strstr(preload, "proxychains") || strstr(preload, "libproxychains")) {
        printf("[*] Detected proxychains environment\n");
        return 1;  /* proxychains mode */
    }
    
    if (strstr(preload, "torsocks") || strstr(preload, "libtorsocks")) {
        printf("[*] Detected torsocks environment\n");
        return 2;  /* torsocks mode */
    }
    
    return 0;  /* Unknown proxy */
}

int proxy_compat_enable(void) {
    if (g_config->proxy_compat_mode == 0) {
        return 0;  /* Not in proxy mode */
    }
    
    printf("[*] Proxy compatibility mode activated\n");
    printf("[!] Forcing TCP CONNECT scan (raw sockets unavailable)\n");
    
    /* Force TCP CONNECT scan for proxy compatibility */
    g_config->scan_type = SCAN_TYPE_CONNECT;
    
    return 0;
}

int check_kernel_features(void) {
    if (g_config->debug) {
        printf("[DEBUG] Checking kernel features...\n");
    }
    
    /* Check for io_uring support */
    if (access("/sys/kernel/config/io_uring", F_OK) == 0) {
        printf("[+] io_uring support detected\n");
    }
    
    /* Check for AF_XDP support */
    if (access("/sys/kernel/config/xdp", F_OK) == 0) {
        printf("[+] AF_XDP support detected\n");
    }
    
    return 0;
}

int fallback_degradation(void) {
    printf("[!] Attempting graceful fallback...\n");
    
    /* Fallback chain: io_uring -> epoll -> select */
    switch (g_config->io_engine) {
        case IO_ENGINE_URING:
            printf("[!] io_uring not available, falling back to EPOLL\n");
            g_config->io_engine = IO_ENGINE_EPOLL;
            break;
        case IO_ENGINE_XDP:
            printf("[!] AF_XDP not available, falling back to io_uring\n");
            g_config->io_engine = IO_ENGINE_URING;
            break;
        case IO_ENGINE_EPOLL:
            printf("[!] EPOLL not available, falling back to SELECT\n");
            g_config->io_engine = IO_ENGINE_SELECT;
            break;
        default:
            return -1;  /* No fallback available */
    }
    
    return 0;
}
