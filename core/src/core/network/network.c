/* ====================================================================
   BLACKMAP 3.0 - NETWORK ENGINE IMPLEMENTATION
   
   High-performance non-blocking network I/O with epoll
   ===================================================================== */

#include "blackmap3/network.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFER_POOL_SIZE 256
#define DEFAULT_BUFFER_CAPACITY 4096

/* ====================================================================
   INTERNAL STRUCTURES
   ==================================================================== */

typedef struct {
    uint8_t buffer[DEFAULT_BUFFER_CAPACITY];
    int in_use;
} buffer_chunk_t;

struct network_engine_internal {
    network_engine_t public;
    
    // Epoll state
    struct epoll_event *epoll_events;
    
    // Connection tracking (currently only used in cleanup)
    connection_t **connections;  // Hash table of active connections
    uint32_t connections_size;
    uint32_t connections_count;
    
    // Completed connection list (for caller to collect results)
    connection_t **completed;      // dynamic array of finished connections
    uint32_t completed_capacity;
    uint32_t completed_count;

    // Buffer pool for banners
    buffer_chunk_t buffer_pool[BUFFER_POOL_SIZE];
    uint32_t buffer_available;
    
    // Timing
    struct timespec engine_start_time;
};

/* ====================================================================
   HELPER FUNCTIONS
   ==================================================================== */

static inline uint64_t timespec_diff_us(struct timespec *a, struct timespec *b) {
    return ((a->tv_sec - b->tv_sec) * 1000000UL) + 
           ((a->tv_nsec - b->tv_nsec) / 1000);
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static uint8_t* buffer_pool_allocate(struct network_engine_internal *eng) {
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!eng->buffer_pool[i].in_use) {
            eng->buffer_pool[i].in_use = 1;
            eng->buffer_available--;
            return eng->buffer_pool[i].buffer;
        }
    }
    return NULL;
}

static void buffer_pool_release(struct network_engine_internal *eng, uint8_t *buf) {
    ptrdiff_t idx = ((uint8_t*)buf - (uint8_t*)eng->buffer_pool) / sizeof(buffer_chunk_t);
    if (idx >= 0 && idx < BUFFER_POOL_SIZE) {
        eng->buffer_pool[idx].in_use = 0;
        eng->buffer_available++;
    }
}

/* ====================================================================
   NETWORK ENGINE API IMPLEMENTATION
   ==================================================================== */

network_engine_t* network_engine_init(
    uint32_t max_concurrency_global,
    uint32_t max_concurrency_per_host,
    uint32_t default_timeout_ms)
{
    struct network_engine_internal *eng = 
        (struct network_engine_internal *)malloc(sizeof(*eng));
    
    if (!eng) return NULL;
    
    memset(eng, 0, sizeof(*eng));
    
    // Initialize public fields
    eng->public.max_concurrency_global = max_concurrency_global;
    eng->public.max_concurrency_per_host = max_concurrency_per_host;
    eng->public.default_timeout_ms = default_timeout_ms;
    eng->public.socket_buffer_size = 65536;
    eng->public.epoll_max_events = 256;
    
    // Create epoll
    eng->public.epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (eng->public.epoll_fd < 0) {
        free(eng);
        return NULL;
    }
    
    // Allocate epoll events array
    eng->epoll_events = 
        (struct epoll_event *)malloc(sizeof(struct epoll_event) * eng->public.epoll_max_events);
    
    if (!eng->epoll_events) {
        close(eng->public.epoll_fd);
        free(eng);
        return NULL;
    }
    
    // Initialize connection tracking
    eng->connections_size = 1024;
    eng->connections = 
        (connection_t **)calloc(eng->connections_size, sizeof(connection_t*));
    
    if (!eng->connections) {
        free(eng->epoll_events);
        close(eng->public.epoll_fd);
        free(eng);
        return NULL;
    }
    
    // Initialize buffer pool
    eng->buffer_available = BUFFER_POOL_SIZE;

    // Initialize completed list
    eng->completed_capacity = 1024;
    eng->completed = (connection_t**)malloc(sizeof(connection_t*) * eng->completed_capacity);
    eng->completed_count = 0;
    
    // Record startup time
    clock_gettime(CLOCK_MONOTONIC, &eng->engine_start_time);
    
    return (network_engine_t*)eng;
}

void network_engine_cleanup(network_engine_t *engine) {
    if (!engine) return;
    
    struct network_engine_internal *eng = (struct network_engine_internal*)engine;
    
    // Close all active connections
    for (uint32_t i = 0; i < eng->connections_size; i++) {
        if (eng->connections[i]) {
            if (eng->connections[i]->fd >= 0) {
                close(eng->connections[i]->fd);
            }
            if (eng->connections[i]->read_buffer) {
                buffer_pool_release(eng, eng->connections[i]->read_buffer);
            }
            free(eng->connections[i]);
        }
    }

    // Free completed list storage (caller is responsible for freeing each connection)
    if (eng->completed) free(eng->completed);
    
    // Free memory
    if (eng->connections) free(eng->connections);
    if (eng->epoll_events) free(eng->epoll_events);
    if (engine->epoll_fd >= 0) close(engine->epoll_fd);
    
    free(eng);
}

connection_t* connection_create(
    const char *ip,
    uint16_t port,
    uint32_t timeout_ms)
{
    connection_t *conn = (connection_t*)malloc(sizeof(*conn));
    if (!conn) return NULL;
    
    memset(conn, 0, sizeof(*conn));
    
    conn->fd = -1;
    conn->state = CONN_STATE_INIT;
    conn->port = port;
    conn->timeout_ms = timeout_ms;
    
    // Parse IP and create address
    if (inet_pton(AF_INET, ip, &conn->addr.sin_addr) != 1) {
        free(conn);
        return NULL;
    }
    
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_port = htons(port);
    
    // Initialize timing
    clock_gettime(CLOCK_MONOTONIC, &conn->start_time);
    
    return conn;
}

void connection_free(connection_t *conn) {
    if (!conn) return;
    if (conn->fd >= 0) close(conn->fd);
    if (conn->read_buffer) free(conn->read_buffer);
    free(conn);
}

int network_queue_connection(network_engine_t *engine, connection_t *conn) {
    if (!engine || !conn || conn->fd >= 0) {
        return -1;
    }
    
    struct network_engine_internal *eng = (struct network_engine_internal*)engine;
    
    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        conn->error_code = errno;
        conn->state = CONN_STATE_ERROR;
        return -1;
    }
    
    // Set non-blocking
    if (set_nonblocking(fd) < 0) {
        close(fd);
        conn->error_code = errno;
        conn->state = CONN_STATE_ERROR;
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    // Allocate read buffer
    conn->read_buffer = buffer_pool_allocate(eng);
    if (!conn->read_buffer) {
        close(fd);
        conn->state = CONN_STATE_ERROR;
        return -1;
    }
    conn->buffer_capacity = DEFAULT_BUFFER_CAPACITY;
    
    conn->fd = fd;
    conn->state = CONN_STATE_CONNECTING;
    
    // Register with epoll
    struct epoll_event ev;
    ev.events = EPOLLOUT | EPOLLERR;  // Watch for writable (connection complete)
    ev.data.ptr = (void*)conn;
    
    if (epoll_ctl(engine->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        close(fd);
        buffer_pool_release(eng, conn->read_buffer);
        conn->read_buffer = NULL;
        conn->error_code = errno;
        conn->state = CONN_STATE_ERROR;
        return -1;
    }
    
    // Try to connect (will return EINPROGRESS for non-blocking)
    int ret = connect(fd, (struct sockaddr*)&conn->addr, sizeof(conn->addr));
    if (ret < 0 && errno != EINPROGRESS) {
        epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
        close(fd);
        buffer_pool_release(eng, conn->read_buffer);
        conn->read_buffer = NULL;
        conn->error_code = errno;
        conn->state = CONN_STATE_ERROR;
        return -1;
    }
    
    // Record metrics
    engine->total_connections_attempted++;
    
    return 0;
}

int network_process_batch(
    network_engine_t *engine,
    uint32_t timeout_logic_ms)
{
    if (!engine) return -1;
    
    struct network_engine_internal *eng = (struct network_engine_internal*)engine;
    
    // Wait for events
    int nfds = epoll_wait(
        engine->epoll_fd,
        eng->epoll_events,
        engine->epoll_max_events,
        timeout_logic_ms
    );
    
    if (nfds < 0) {
        if (errno == EINTR) return 0;  // Interrupted, not error
        return -1;
    }
    
    // Process all events
    for (int i = 0; i < nfds; i++) {
        struct epoll_event *ev = &eng->epoll_events[i];
        connection_t *conn = (connection_t*)ev->data.ptr;
        
        if (!conn) continue;
        
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        conn->elapsed_ms = timespec_diff_us(&now, &conn->start_time) / 1000;
        
        // Check timeout
        if (conn->elapsed_ms > conn->timeout_ms) {
            epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            conn->fd = -1;
            conn->state = CONN_STATE_TIMEOUT;
            engine->total_timeouts++;
            // record as completed for caller
            if (eng->completed_count >= eng->completed_capacity) {
                uint32_t newcap = eng->completed_capacity * 2;
                eng->completed = (connection_t**)realloc(eng->completed, sizeof(connection_t*) * newcap);
                eng->completed_capacity = newcap;
            }
            eng->completed[eng->completed_count++] = conn;
            continue;
        }
        
        // Check for errors
        if (ev->events & EPOLLERR) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            
            epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            conn->fd = -1;
            conn->error_code = err;
            conn->state = CONN_STATE_ERROR;
            engine->total_errors++;
            continue;
        }
        
        // Connection completed
        if (conn->state == CONN_STATE_CONNECTING && (ev->events & EPOLLOUT)) {
            int err = 0;
            socklen_t errlen = sizeof(err);
            getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            
            if (err != 0) {
                epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
                close(conn->fd);
                conn->fd = -1;
                conn->error_code = err;
                conn->state = CONN_STATE_ERROR;
                engine->total_errors++;
                if (eng->completed_count >= eng->completed_capacity) {
                    uint32_t newcap = eng->completed_capacity * 2;
                    eng->completed = (connection_t**)realloc(eng->completed, sizeof(connection_t*) * newcap);
                    eng->completed_capacity = newcap;
                }
                eng->completed[eng->completed_count++] = conn;
                continue;
            }
            
            // Connection successful
            clock_gettime(CLOCK_MONOTONIC, &conn->connect_time);
            conn->rtt_us = timespec_diff_us(&conn->connect_time, &conn->start_time);
            
            conn->state = CONN_STATE_OPEN;
            engine->total_connections_successful++;
            
            // For TCP connect probes, close immediately after connection
            if (conn->probe_type == PROBE_TCP_CON) {
                epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
                close(conn->fd);
                conn->fd = -1;
                // Add to completed list
                if (eng->completed_count >= eng->completed_capacity) {
                    uint32_t newcap = eng->completed_capacity * 2;
                    eng->completed = (connection_t**)realloc(eng->completed, sizeof(connection_t*) * newcap);
                    eng->completed_capacity = newcap;
                }
                eng->completed[eng->completed_count++] = conn;
                continue;
            }
            
            // For other probes, modify epoll to watch for readable (banner data)
            struct epoll_event new_ev;
            new_ev.events = EPOLLIN | EPOLLERR;
            new_ev.data.ptr = (void*)conn;
            epoll_ctl(engine->epoll_fd, EPOLL_CTL_MOD, conn->fd, &new_ev);
            
            conn->state = CONN_STATE_READING;
            continue;
        }
        
        // Read banner data
        if (conn->state == CONN_STATE_READING && (ev->events & EPOLLIN)) {
            ssize_t n = recv(
                conn->fd,
                conn->read_buffer + conn->read_bytes,
                conn->buffer_capacity - conn->read_bytes,
                0
            );
            
            if (n > 0) {
                conn->read_bytes += n;
                engine->bytes_received += n;
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                conn->state = CONN_STATE_ERROR;
            } else {
                // Got FIN or EAGAIN - close and mark complete
                conn->state = CONN_STATE_CLOSED;
            }
            
            // Close connection after reading banner
            epoll_ctl(engine->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            close(conn->fd);
            conn->fd = -1;
        }
    }
    
    return nfds;
}

conn_state_t connection_get_state(connection_t *conn) {
    if (!conn) return CONN_STATE_ERROR;
    return conn->state;
}

uint64_t connection_get_rtt_us(connection_t *conn) {
    if (!conn) return 0;
    return conn->rtt_us;
}

const uint8_t* connection_get_banner(connection_t *conn, size_t *out_size) {
    if (!conn || !out_size) return NULL;
    *out_size = conn->read_bytes;
    return conn->read_buffer;
}

int connection_get_error(connection_t *conn) {
    if (!conn) return -1;
    return conn->error_code;
}

network_metrics_t network_get_metrics(network_engine_t *engine) {
    network_metrics_t metrics = {0};
    
    if (!engine) return metrics;
    
    metrics.total_attempted = engine->total_connections_attempted;
    metrics.total_successful = engine->total_connections_successful;
    metrics.total_timeouts = engine->total_timeouts;
    metrics.total_errors = engine->total_errors;
    metrics.bytes_sent = engine->bytes_sent;
    metrics.bytes_received = engine->bytes_received;
    
    if (metrics.total_attempted > 0) {
        metrics.avg_rtt_us = (metrics.total_successful > 0) ? 
            (metrics.bytes_received / metrics.total_successful) : 0;
    }
    
    return metrics;
}

int network_collect_finished(network_engine_t *engine,
                             connection_t ***out_list,
                             uint32_t *out_count)
{
    if (!engine || !out_list || !out_count) return -1;
    
    struct network_engine_internal *eng = (struct network_engine_internal*)engine;
    *out_list = eng->completed;
    *out_count = eng->completed_count;
    
    /* reset completed count; caller now owns returned pointers */
    eng->completed_count = 0;
    return 0;
}
