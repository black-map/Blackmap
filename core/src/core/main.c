#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "blackmap.h"
#include "cli.h"
#include "logging.h"

blackmap_config_t *g_config = NULL;

static void signal_handler(int sig) {
    fprintf(stderr, "\n[!] Caught signal %d, cleaning up...\n", sig);
    blackmap_cleanup();
    log_close();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    /* Initialize config */
    g_config = calloc(1, sizeof(blackmap_config_t));
    if (!g_config) {
        perror("malloc");
        return EXIT_FAILURE;
    }
    
    /* Parse command line */
    int parse_result = parse_command_line(argc, argv, g_config);
    if (parse_result == 1) {
        // Help or version printed, exit success
        free(g_config);
        return EXIT_SUCCESS;
    } else if (parse_result == -1) {
        free(g_config);
        return EXIT_FAILURE;
    }
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize blackmap */
    if (blackmap_init() != 0) {
        fprintf(stderr, "Failed to initialize BlackMap\n");
        free(g_config);
        return EXIT_FAILURE;
    }
    
    /* Run scan */
    if (blackmap_run() != 0) {
        fprintf(stderr, "Scan failed\n");
        blackmap_cleanup();
        log_close();
        return EXIT_FAILURE;
    }
    
    blackmap_cleanup();
    log_close();
    return EXIT_SUCCESS;
}
