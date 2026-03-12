#ifndef ARGUMENT_PARSER_H
#define ARGUMENT_PARSER_H

#include "../include/common.h"

typedef struct {
    char target[256];
    char *port_str;
    scan_type_t scan_type;
    int timeout;
    int threads;
    int rate_limit;
    int version_detect;
    int os_detect;
    output_format_t output_format;
    char output_file[256];
    int verbose;
    int timing;
} scan_config_t;

void parse_arguments(int argc, char *argv[], scan_config_t *config);
void usage(const char *prog);
void parse_ports(const char *port_str, int **ports, int *port_count);

#endif
