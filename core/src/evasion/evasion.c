#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "blackmap.h"

/* Evasion Techniques */

int fragmentation_init(uint16_t mtu) {
    if (mtu < 20 || mtu > 65535) {
        fprintf(stderr, "[-] Invalid MTU: %u (must be 20-65535)\n", mtu);
        return -1;
    }
    
    printf("[*] Fragmentation enabled with MTU: %u\n", mtu);
    return 0;
}

int build_decoy_list(void) {
    if (g_config->num_decoys == 0) {
        return 0;
    }
    
    printf("[*] Using %u decoys:\n", g_config->num_decoys);
    for (uint32_t i = 0; i < g_config->num_decoys; i++) {
        printf("   - %s\n", inet_ntoa(g_config->decoys[i]));
    }
    
    return 0;
}

int timing_setup(timing_template_t timing) {
    const char *timing_names[] = {
        "Paranoid (-T0)", "Sneaky (-T1)", "Polite (-T2)",
        "Normal (-T3)", "Aggressive (-T4)", "Insane (-T5)"
    };
    
    if (timing >= sizeof(timing_names) / sizeof(timing_names[0])) {
        return -1;
    }
    
    printf("[*] Timing template: %s\n", timing_names[timing]);
    return 0;
}

int personality_set(const char *os) {
    printf("[*] OS personality: %s\n", os);
    return 0;
}

uint16_t randomize_source_port(void) {
    if (g_config->source_port == 0) {
        srand(time(NULL));
        return (rand() % 65535) + 1;
    }
    return g_config->source_port;
}

void apply_scan_delay(void) {
    if (g_config->scan_delay_ms > 0) {
        usleep(g_config->scan_delay_ms * 1000);
    }
}

void slow_stealth_mode(void) {
    if (g_config->slow_stealth) {
        // Increase delays, randomize more
        g_config->scan_delay_ms = 1000; // 1 second
        g_config->max_scan_delay_ms = 5000;
        if (g_config->verbosity > 0) {
            printf("[*] Slow stealth mode enabled\n");
        }
    }
}

int payload_obfuscation(void) {
    if (!g_config->payload) {
        /* Generate random payload */
        g_config->payload_len = 32 + (rand() % 224);
        g_config->payload = malloc(g_config->payload_len);
        
        for (uint32_t i = 0; i < g_config->payload_len; i++) {
            g_config->payload[i] = rand() & 0xFF;
        }
        
        printf("[*] Payload obfuscation enabled (%u bytes)\n", g_config->payload_len);
    }
    
    return 0;
}
