#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include "engines_internal.h"

/* SELECT engine - fallback for legacy systems */

static int select_engine_init(void) {
    printf("[*] SELECT engine: Using fallback select() mechanism\n");
    return 0;
}

static void select_engine_cleanup(void) {
    printf("[*] SELECT engine: Cleanup\n");
}

static int select_engine_submit(const uint8_t *packet, uint32_t len) {
    /* Stub: Send packet through raw socket */
    (void)packet;
    (void)len;
    return 0;
}

static int select_engine_get_responses(uint8_t *buffer, uint32_t max_len, 
                                       uint32_t *packets) {
    /* Stub: Receive responses */
    (void)buffer;
    (void)max_len;
    *packets = 0;
    return 0;
}

static int select_engine_poll(uint32_t timeout_ms) {
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    
    /* Would add actual socket FDs here */
    return select(0, &readfds, NULL, NULL, &tv);
}

static bool select_engine_supported(void) {
    return true; /* Always available */
}

static io_engine_t select_engine = {
    .name = "select",
    .init = select_engine_init,
    .cleanup = select_engine_cleanup,
    .submit_packet = select_engine_submit,
    .get_responses = select_engine_get_responses,
    .poll_timeout = select_engine_poll,
    .is_supported = select_engine_supported
};

io_engine_t *engine_get_select(void) {
    return &select_engine;
}
