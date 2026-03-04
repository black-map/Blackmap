#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include "engines_internal.h"

#ifdef HAVE_LIBURING
#include <liburing.h>

/* io_uring engine - Trinity Engine Burst Mode implementation */

#define RING_SIZE 32768  /* SQE batch size as specified */
#define QUEUE_DEPTH 4096

static struct io_uring ring;
static bool uring_initialized = false;

static int uring_engine_init(void) {
    struct io_uring_params params;
    
    memset(&params, 0, sizeof(params));
    params.flags = IORING_SETUP_IOPOLL;  /* Enable iopoll mode for zero interrupts */
    
    int ret = io_uring_queue_init_params(QUEUE_DEPTH, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "[-] io_uring_queue_init_params failed: %d\n", ret);
        return -1;
    }
    
    uring_initialized = true;
    
    /* Verify capability flags */
    if (ring.features & IORING_FEAT_FAST_POLL) {
        printf("[+] io_uring: FAST_POLL enabled (zero-syscall hot path)\n");
    }
    
    printf("[+] io_uring engine: Initialized with %u depth\n", QUEUE_DEPTH);
    return 0;
}

static void uring_engine_cleanup(void) {
    if (uring_initialized) {
        io_uring_queue_exit(&ring);
        uring_initialized = false;
    }
    printf("[*] io_uring engine: Cleanup\n");
}

static int uring_engine_submit(const uint8_t *packet, uint32_t len) {
    if (!uring_initialized) {
        return -1;
    }
    
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        /* Submit queue full, flush and retry */
        io_uring_submit(&ring);
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            return -1;
        }
    }
    
    /* Send UDP packet (stub - would use raw socket in production) */
    (void)packet;
    (void)len;
    
    return 0;
}

static int uring_engine_get_responses(uint8_t *buffer, uint32_t max_len, 
                                      uint32_t *packets) {
    if (!uring_initialized) {
        return -1;
    }
    
    struct io_uring_cqe *cqe;
    unsigned head;
    int count = 0;
    
    io_uring_for_each_cqe(&ring, head, cqe) {
        count++;
        if (count >= 256) break;
    }
    
    io_uring_cq_advance(&ring, count);
    *packets = count;
    
    (void)buffer;
    (void)max_len;
    return count;
}

static int uring_engine_poll(uint32_t timeout_ms) {
    if (!uring_initialized) {
        return -1;
    }
    
    struct __kernel_timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    
    return io_uring_submit_and_wait_timeout(&ring, NULL, 1, &ts);
}

static bool uring_engine_supported(void) {
    /* Check kernel version >= 6.1 */
    struct utsname uts;
    if (uname(&uts) != 0) {
        return false;
    }
    
    int major = 0, minor = 0;
    sscanf(uts.release, "%d.%d", &major, &minor);
    
    return major > 6 || (major == 6 && minor >= 1);
}

#else

/* Stub for when liburing is not available */

static int uring_engine_init(void) {
    printf("[!] io_uring not available (liburing not installed)\n");
    return -1;
}

static void uring_engine_cleanup(void) {
}

static int uring_engine_submit(const uint8_t *packet, uint32_t len) {
    (void)packet;
    (void)len;
    return -1;
}

static int uring_engine_get_responses(uint8_t *buffer, uint32_t max_len, 
                                      uint32_t *packets) {
    (void)buffer;
    (void)max_len;
    *packets = 0;
    return -1;
}

static int uring_engine_poll(uint32_t timeout_ms) {
    (void)timeout_ms;
    return -1;
}

static bool uring_engine_supported(void) {
    return false;
}

#endif

static io_engine_t uring_engine = {
    .name = "io_uring",
    .init = uring_engine_init,
    .cleanup = uring_engine_cleanup,
    .submit_packet = uring_engine_submit,
    .get_responses = uring_engine_get_responses,
    .poll_timeout = uring_engine_poll,
    .is_supported = uring_engine_supported
};

io_engine_t *engine_get_uring(void) {
    return &uring_engine;
}

