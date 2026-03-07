#ifndef CLI_H
#define CLI_H

#include "blackmap.h"

/* CLI parsing functions */
int parse_command_line(int argc, char *argv[], blackmap_config_t *config);
void print_usage(const char *prog);
void print_version(void);

#endif /* CLI_H */