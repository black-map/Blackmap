#include "../include/common.h"
#include "../include/argument_parser.h"
#include "../include/scanners.h"
#include "../include/service_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define MAX_RESULTS 65536

volatile int running = 1;
int verbose_mode = 0;

static scan_result_t results[MAX_RESULTS];
static int result_count = 0;
static pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;

double get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

void print_result(const char *ip, int port, port_state_t state, scan_result_t *result, output_format_t format) {
    const char *state_str;
    switch (state) {
        case PORT_OPEN: state_str = "OPEN"; break;
        case PORT_CLOSED: state_str = "CLOSED"; break;
        case PORT_FILTERED: state_str = "FILTERED"; break;
        case PORT_OPEN_FILTERED: state_str = "OPEN|FILTERED"; break;
        case PORT_UNFILTERED: state_str = "UNFILTERED"; break;
        default: state_str = "UNKNOWN";
    }
    
    if (format == FORMAT_NORMAL) {
        printf("%-8d/%-4s %-10s %s", port, "tcp", state_str, result->service);
        if (result->version[0] != '\0') {
            printf(" %s", result->version);
        }
        printf("\n");
    } else if (format == FORMAT_JSON) {
        static int first = 1;
        if (!first) printf(",\n");
        first = 0;
        printf("  {\"port\": %d, \"protocol\": \"tcp\", \"state\": \"%s\", \"service\": \"%s\", \"version\": \"%s\"}",
               port, state_str, result->service, result->version);
    } else if (format == FORMAT_GREPEABLE) {
        printf("PORT|%d/tcp|%s|%s", port, state_str, result->service);
        if (result->version[0] != '\0') {
            printf("|%s", result->version);
        }
        printf("\n");
    } else if (format == FORMAT_XML) {
        printf("  <port><number>%d</number><protocol>tcp</protocol><state>%s</state><service>%s</service></port>\n",
               port, state_str, result->service);
    }
}

void print_nmap_style(const char *ip, scan_result_t *results, int count, double duration) {
    printf("\nNmap scan report for %s\n", ip);
    printf("Host is %s (%.4fs latency).\n", count > 0 ? "up" : "down", duration);
    printf("\n");
    printf("PORT     STATE         SERVICE\n");
    
    for (int i = 0; i < count; i++) {
        if (results[i].state == PORT_OPEN) {
            printf("%-8d/tcp %-12s %s", results[i].port, "open", results[i].service);
            if (results[i].version[0] != '\0') {
                printf(" %s", results[i].version);
            }
            printf("\n");
        }
    }
    
    printf("\nBlackmap done: 1 IP address scanned in %.2f seconds\n", duration);
}

void* scan_worker(void *arg) {
    scan_task_t *task = (scan_task_t *)arg;
    
    scan_func_t scan_func = get_scan_function(task->scan_type);
    port_state_t state = scan_func(task->target, task->port_start, task->timeout);
    
    scan_result_t result;
    memset(&result, 0, sizeof(result));
    strncpy(result.ip, task->target, INET_ADDRSTRLEN - 1);
    result.port = task->port_start;
    result.state = state;
    strncpy(result.service, get_service_name(task->port_start), 63);
    result.response_time = 0;
    
    pthread_mutex_lock(&results_mutex);
    if (result_count < MAX_RESULTS) {
        results[result_count++] = result;
    }
    pthread_mutex_unlock(&results_mutex);
    
    free(task);
    return NULL;
}

int main(int argc, char *argv[]) {
    scan_config_t config;
    parse_arguments(argc, argv, &config);
    
    signal(SIGINT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));
    
    char ip[INET_ADDRSTRLEN];
    struct in_addr addr;
    
    if (inet_pton(AF_INET, config.target, &addr) == 1) {
        inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN);
    } else {
        struct hostent *he = gethostbyname(config.target);
        if (!he) {
            fprintf(stderr, "Error: Could not resolve %s\n", config.target);
            return 1;
        }
        struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
        inet_ntop(AF_INET, addr_list[0], ip, INET_ADDRSTRLEN);
    }
    
    if (config.verbose) {
        printf("[*] Blackmap v1.2\n");
        printf("[*] Target: %s (%s)\n", config.target, ip);
        printf("[*] Scan Type: %d\n", config.scan_type);
        printf("[*] Timing: T%d (timeout: %d ms, threads: %d)\n", config.timing, config.timeout, config.threads);
    }
    
    int *ports = NULL;
    int port_count = 0;
    parse_ports(config.port_str, &ports, &port_count);
    
    if (config.verbose) {
        printf("[*] Ports to scan: %d\n", port_count);
    }
    
    FILE *output_fp = stdout;
    int file_output = 0;
    if (config.output_file[0] != '\0') {
        output_fp = fopen(config.output_file, "w");
        file_output = 1;
    }
    
    if (config.output_format == FORMAT_JSON) {
        fprintf(output_fp, "{\n");
        fprintf(output_fp, "  \"scanner\": \"blackmap\",\n");
        fprintf(output_fp, "  \"version\": \"1.2\",\n");
        fprintf(output_fp, "  \"target\": \"%s\",\n", ip);
        fprintf(output_fp, "  \"results\": [\n");
    } else if (config.output_format == FORMAT_XML) {
        fprintf(output_fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(output_fp, "<nmaprun scanner=\"blackmap\" version=\"1.2\">\n");
        fprintf(output_fp, "<host><address addr=\"%s\" addrtype=\"ipv4\"/></host>\n", ip);
        fprintf(output_fp, "<ports>\n");
    }
    
    pthread_t scan_threads[MAX_THREADS];
    int thread_idx = 0;
    int open_count = 0;
    
    double scan_start = get_timestamp_ms();
    
    for (int i = 0; i < port_count && running; i++) {
        scan_task_t *task = malloc(sizeof(scan_task_t));
        strncpy(task->target, ip, 255);
        task->port_start = ports[i];
        task->timeout = config.timeout;
        task->scan_type = config.scan_type;
        
        pthread_create(&scan_threads[thread_idx], NULL, scan_worker, task);
        thread_idx++;
        
        if (thread_idx >= config.threads || i == port_count - 1) {
            for (int j = 0; j < thread_idx; j++) {
                pthread_join(scan_threads[j], NULL);
            }
            thread_idx = 0;
        }
    }
    
    double scan_end = get_timestamp_ms();
    double duration = (scan_end - scan_start) / 1000.0;
    
    for (int i = 0; i < result_count; i++) {
        if (results[i].state == PORT_OPEN) {
            open_count++;
            if (config.version_detect) {
                detect_service(ip, results[i].port, config.timeout, &results[i]);
            }
        }
    }
    
    if (config.output_format == FORMAT_NORMAL && config.verbose) {
        print_nmap_style(ip, results, result_count, duration);
    } else {
        for (int i = 0; i < result_count; i++) {
            if (results[i].state == PORT_OPEN) {
                print_result(ip, results[i].port, results[i].state, &results[i], config.output_format);
            }
        }
    }
    
    if (config.output_format == FORMAT_JSON) {
        fprintf(output_fp, "\n  ],\n");
        fprintf(output_fp, "  \"open_ports\": %d,\n", open_count);
        fprintf(output_fp, "  \"scan_duration\": %.2f\n", duration);
        fprintf(output_fp, "}\n");
    } else if (config.output_format == FORMAT_XML) {
        for (int i = 0; i < result_count; i++) {
            if (results[i].state == PORT_OPEN) {
                fprintf(output_fp, "  <port><protocol>tcp</protocol><portid>%d</portid><state state=\"open\"/></port>\n",
                       results[i].port);
            }
        }
        fprintf(output_fp, "</ports>\n</nmaprun>\n");
    }
    
    free(ports);
    
    if (file_output && output_fp != stdout) {
        fclose(output_fp);
    }
    
    if (config.verbose) {
        printf("\n[*] Scan completed. Open ports: %d\n", open_count);
        printf("[*] Scan duration: %.2f seconds\n", duration);
    }
    
    return 0;
}
