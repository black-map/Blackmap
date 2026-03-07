#ifndef ENGINES_H
#define ENGINES_H

#include <stdint.h>
#include <stdbool.h>

/* IO Engine Interface */
typedef struct io_engine {
    const char *name;
    int (*init)(void);
    void (*cleanup)(void);
    int (*submit_packet)(const uint8_t *packet, uint32_t len);
    int (*get_responses)(uint8_t *buffer, uint32_t max_len, uint32_t *packets);
    int (*poll_timeout)(uint32_t timeout_ms);
    bool (*is_supported)(void);
} io_engine_t;

/* Exported functions */
io_engine_t *engine_get_uring(void);
io_engine_t *engine_get_xdp(void);
io_engine_t *engine_get_epoll(void);
io_engine_t *engine_get_select(void);

#endif /* ENGINES_H */

