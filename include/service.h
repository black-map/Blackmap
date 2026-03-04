#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>
#include "blackmap.h"

/* Service detection */
int detect_service(uint32_t ip, uint16_t port, port_info_t *info);
int load_service_signatures(const char *filename);

#endif /* SERVICE_H */