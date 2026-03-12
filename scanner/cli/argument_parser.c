#include "../include/argument_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

void usage(const char *prog) {
    printf("Blackmap v1.2 - Advanced Network Scanner\n");
    printf("==========================================\n\n");
    printf("Usage: %s [OPTIONS] <target>\n\n", prog);
    printf("OPTIONS:\n");
    printf("  -p <ports>       Ports (e.g., 22,80,443 or 1-1000 or all)\n");
    printf("  -s <type>        Scan type: connect|syn|fin|xmas|null|udp|ack\n");
    printf("  -T <1-5>         Timing template (T1=slow, T5=fast)\n");
    printf("  -c <threads>     Concurrent threads (default: 50, max: 500)\n");
    printf("  -r <rate>        Packet rate limit\n");
    printf("  -sV              Service version detection\n");
    printf("  -O               OS detection (requires root)\n");
    printf("  -A               Enable all detections\n");
    printf("  -oN <file>       Normal output\n");
    printf("  -oX <file>       XML output\n");
    printf("  -oJ <file>       JSON output\n");
    printf("  -oG <file>       Grepable output\n");
    printf("  -v               Verbose mode\n");
    printf("  -vv              Very verbose\n");
    printf("  -h               Help\n\n");
    printf("EXAMPLES:\n");
    printf("  %s 192.168.1.1 -p 1-1000 -sS -sV\n", prog);
    printf("  %s 10.0.0.0/24 -p 22,80,443 -sV\n", prog);
    printf("  %s target.com -p 1-10000 -T4\n", prog);
}

void parse_arguments(int argc, char *argv[], scan_config_t *config) {
    memset(config, 0, sizeof(scan_config_t));
    
    config->scan_type = SCAN_CONNECT;
    config->timeout = DEFAULT_TIMEOUT;
    config->threads = 50;
    config->timing = 3;
    config->output_format = FORMAT_NORMAL;
    
    int opt;
    while ((opt = getopt(argc, argv, "p:s:T:c:r:sVo:f:h?vAOS")) != -1) {
        switch (opt) {
            case 'p':
                config->port_str = optarg;
                break;
            case 's':
                if (strcmp(optarg, "syn") == 0) config->scan_type = SCAN_SYN;
                else if (strcmp(optarg, "S") == 0) config->scan_type = SCAN_SYN;
                else if (strcmp(optarg, "fin") == 0) config->scan_type = SCAN_FIN;
                else if (strcmp(optarg, "F") == 0) config->scan_type = SCAN_FIN;
                else if (strcmp(optarg, "xmas") == 0) config->scan_type = SCAN_XMAS;
                else if (strcmp(optarg, "X") == 0) config->scan_type = SCAN_XMAS;
                else if (strcmp(optarg, "null") == 0) config->scan_type = SCAN_NULL;
                else if (strcmp(optarg, "N") == 0) config->scan_type = SCAN_NULL;
                else if (strcmp(optarg, "ack") == 0) config->scan_type = SCAN_ACK;
                else if (strcmp(optarg, "udp") == 0 || strcmp(optarg, "U") == 0) config->scan_type = SCAN_UDP;
                else config->scan_type = SCAN_CONNECT;
                break;
            case 'T':
                if (strlen(optarg) == 1 && optarg[0] >= '1' && optarg[0] <= '5') {
                    config->timing = atoi(optarg);
                    switch (config->timing) {
                        case 1: config->timeout = 30000; config->threads = 5; break;
                        case 2: config->timeout = 15000; config->threads = 10; break;
                        case 3: config->timeout = 8000; config->threads = 50; break;
                        case 4: config->timeout = 4000; config->threads = 150; break;
                        case 5: config->timeout = 2000; config->threads = 300; break;
                    }
                } else {
                    config->timeout = atoi(optarg);
                }
                break;
            case 'c':
                config->threads = atoi(optarg);
                if (config->threads > MAX_THREADS) config->threads = MAX_THREADS;
                break;
            case 'r':
                config->rate_limit = atoi(optarg);
                break;
            case 'V':
                config->version_detect = 1;
                break;
            case 'O':
                config->os_detect = 1;
                break;
            case 'A':
                config->os_detect = 1;
                config->version_detect = 1;
                break;
            case 'o':
                if (optarg[0] == 'N' || optarg[0] == 'n') {
                    config->output_format = FORMAT_NORMAL;
                    strncpy(config->output_file, optarg + 2, 255);
                } else if (optarg[0] == 'X' || optarg[0] == 'x') {
                    config->output_format = FORMAT_XML;
                    strncpy(config->output_file, optarg + 2, 255);
                } else if (optarg[0] == 'J' || optarg[0] == 'j') {
                    config->output_format = FORMAT_JSON;
                    strncpy(config->output_file, optarg + 2, 255);
                } else if (optarg[0] == 'G' || optarg[0] == 'g') {
                    config->output_format = FORMAT_GREPEABLE;
                    strncpy(config->output_file, optarg + 2, 255);
                }
                break;
            case 'v':
                config->verbose++;
                verbose_mode = config->verbose;
                break;
            case 'h':
                usage(argv[0]);
                exit(0);
            default:
                usage(argv[0]);
                exit(1);
        }
    }
    
    if (optind >= argc) {
        usage(argv[0]);
        exit(1);
    }
    
    strncpy(config->target, argv[optind], 255);
    
    if (config->port_str == NULL) {
        config->port_str = "22,80,443";
    }
    
    if (config->scan_type != SCAN_CONNECT && geteuid() != 0) {
        fprintf(stderr, "Warning: Raw scans require root privileges\n");
        config->scan_type = SCAN_CONNECT;
    }
}

void parse_ports(const char *port_str, int **ports, int *port_count) {
    int *ports_arr = NULL;
    int capacity = 1024;
    int count = 0;
    
    ports_arr = malloc(capacity * sizeof(int));
    if (!ports_arr) {
        perror("malloc");
        exit(1);
    }
    
    if (strcmp(port_str, "all") == 0 || strcmp(port_str, "-") == 0) {
        for (int p = 1; p <= 1024 && count < 65535; p++) {
            if (count >= capacity) {
                capacity *= 2;
                ports_arr = realloc(ports_arr, capacity * sizeof(int));
            }
            ports_arr[count++] = p;
        }
    } else {
        char *str = strdup(port_str);
        char *token = strtok(str, ",");
        
        while (token) {
            if (strchr(token, '-')) {
                int start, end;
                sscanf(token, "%d-%d", &start, &end);
                start = (start < 1) ? 1 : start;
                end = (end > 65535) ? 65535 : end;
                for (int p = start; p <= end && count < 65535; p++) {
                    if (count >= capacity) {
                        capacity *= 2;
                        ports_arr = realloc(ports_arr, capacity * sizeof(int));
                    }
                    ports_arr[count++] = p;
                }
            } else {
                int port = atoi(token);
                if (port > 0 && port <= 65535) {
                    if (count >= capacity) {
                        capacity *= 2;
                        ports_arr = realloc(ports_arr, capacity * sizeof(int));
                    }
                    ports_arr[count++] = port;
                }
            }
            token = strtok(NULL, ",");
        }
        free(str);
    }
    
    *ports = ports_arr;
    *port_count = count;
}
