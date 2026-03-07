#ifndef ENGINES_INTERNAL_H
#define ENGINES_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/* Internal engine structures and functions */

typedef struct io_engine {
    const char *name;
    int (*init)(void);
    void (*cleanup)(void);
    int (*submit_packet)(const uint8_t *packet, uint32_t len);
    int (*get_responses)(uint8_t *buffer, uint32_t max_len, uint32_t *packets);
    int (*poll_timeout)(uint32_t timeout_ms);
    bool (*is_supported)(void);
} io_engine_t;

#endif /* ENGINES_INTERNAL_H */
