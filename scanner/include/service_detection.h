#ifndef SERVICE_DETECTION_H
#define SERVICE_DETECTION_H

#include "../include/common.h"

typedef struct {
    int port;
    const char *service;
    const char *product;
    const char *version_pattern;
} service_signature_t;

void detect_service(const char *target_ip, int port, int timeout, scan_result_t *result);
const char* get_service_name(int port);

#endif
