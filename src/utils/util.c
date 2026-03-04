#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "blackmap.h"

/* Utility Functions */

void logging_init(const char *logfile, int level) {
    if (logfile) {
        printf("[*] Logging to: %s\n", logfile);
    }
    
    if (g_config->debug) {
        printf("[DEBUG] Logging level: %d\n", level);
    }
}

void logging_cleanup(void) {
    if (g_config->verbosity > 0) {
        printf("[*] Logging cleanup\n");
    }
}

int random_init(void) {
    srand(time(NULL) ^ getpid());
    return 0;
}

uint32_t random_uint32(void) {
    return (uint32_t)rand();
}

uint8_t random_uint8(void) {
    return (uint8_t)(rand() & 0xFF);
}

void bytearray_shuffle(uint8_t *array, size_t len) {
    for (size_t i = len - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        uint8_t temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}
