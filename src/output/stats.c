#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "blackmap.h"

/* Statistics Tracking */

static unsigned long packets_sent = 0;
static unsigned long packets_received = 0;
static unsigned long hosts_scanned = 0;
static unsigned long ports_scanned = 0;

struct timespec start_time;

int stats_init(void) {
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return 0;
}

void stats_print(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    double elapsed = (now.tv_sec - start_time.tv_sec) + 
                     (now.tv_nsec - start_time.tv_nsec) / 1e9;
    
    printf("\n=== Statistics ===\n");
    printf("Elapsed time: %.2f seconds\n", elapsed);
    printf("Packets sent: %lu\n", packets_sent);
    printf("Packets received: %lu\n", packets_received);
    printf("Hosts scanned: %lu\n", hosts_scanned);
    printf("Ports scanned: %lu\n", ports_scanned);
    
    if (elapsed > 0) {
        double pps = packets_sent / elapsed;
        printf("Throughput: %.0f pps\n", pps);
    }
    printf("==================\n\n");
}

void stats_record_packet_sent(unsigned long count) {
    packets_sent += count;
}

void stats_record_packet_received(unsigned long count) {
    packets_received += count;
}

void stats_record_host(void) {
    hosts_scanned++;
}
