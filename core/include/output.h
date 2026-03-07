#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <stdio.h>
#include "blackmap.h"

/* Output formats */
typedef int (*output_handler_t)(FILE *fp, host_info_t *host);

int output_normal(FILE *fp, host_info_t *host);
int output_xml(FILE *fp, host_info_t *host);
int output_grep(FILE *fp, host_info_t *host);
int output_json(FILE *fp, host_info_t *host);
int output_sqlite(FILE *fp, host_info_t *host);
int output_html(FILE *fp, host_info_t *host);
int output_markdown(FILE *fp, host_info_t *host);

int output_write_results(host_info_t **hosts, uint32_t num_hosts,
                         const blackmap_config_t *config);

#endif /* OUTPUT_H */
