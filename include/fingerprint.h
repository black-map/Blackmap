#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <stdint.h>
#include "blackmap.h"

/* OS Detection */
typedef struct {
    const char *os_name;
    uint32_t confidence;
    const char *fingerprint_type;
} os_detection_result_t;

/* Service Detection */
typedef struct {
    const char *service_name;
    const char *version;
    uint32_t confidence;
    const char *detection_method;
} service_detection_result_t;

/* Functions */
int os_detect(host_info_t *host, os_detection_result_t *result);
int service_detect(host_info_t *host, port_info_t *port, 
                   service_detection_result_t *result);
int version_detect(host_info_t *host, const char *service);

int load_fingerprint_database(const char *db_path);
void cleanup_fingerprint_database(void);

#endif /* FINGERPRINT_H */
