#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>
#include "engines_internal.h"

/* EPOLL engine - modern recursive I/O multiplexing */

static int epoll_fd = -1;

static int epoll_engine_init(void) {
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        return -1;
    }
    
    printf("[*] EPOLL engine: Initialized with fd %d\n", epoll_fd);
    return 0;
}

static void epoll_engine_cleanup(void) {
    if (epoll_fd != -1) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    printf("[*] EPOLL engine: Cleanup\n");
}

static int epoll_engine_submit(const uint8_t *packet, uint32_t len) {
    /* Stub: Queue packet for transmission */
    (void)packet;
    (void)len;
    return 0;
}

static int epoll_engine_get_responses(uint8_t *buffer, uint32_t max_len, 
                                      uint32_t *packets) {
    struct epoll_event events[256];
    int nfds = epoll_wait(epoll_fd, events, 256, 0);
    
    if (nfds < 0) {
        perror("epoll_wait");
        return -1;
    }
    
    *packets = (uint32_t)nfds;
    (void)buffer;
    (void)max_len;
    return nfds;
}

static int epoll_engine_poll(uint32_t timeout_ms) {
    struct epoll_event events[256];
    return epoll_wait(epoll_fd, events, 256, (int)timeout_ms);
}

static bool epoll_engine_supported(void) {
    /* Test if epoll is available */
    int fd = epoll_create1(EPOLL_CLOEXEC);
    if (fd == -1) {
        return false;
    }
    close(fd);
    return true;
}

static io_engine_t epoll_engine = {
    .name = "epoll",
    .init = epoll_engine_init,
    .cleanup = epoll_engine_cleanup,
    .submit_packet = epoll_engine_submit,
    .get_responses = epoll_engine_get_responses,
    .poll_timeout = epoll_engine_poll,
    .is_supported = epoll_engine_supported
};

io_engine_t *engine_get_epoll(void) {
    return &epoll_engine;
}
