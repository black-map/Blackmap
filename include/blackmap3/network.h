#ifndef BLACKMAP3_NETWORK_H
#define BLACKMAP3_NETWORK_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

/* ====================================================================
   NETWORK ENGINE 3.0 ARCHITECTURE
   
   - Non-blocking sockets with epoll for true async I/O
   - Per-connection state machine
   - Individual timeout tracking
   - Real RTT measurement
   - Buffer pooling for efficiency
   ==================================================================== */

typedef enum {
    CONN_STATE_INIT        = 0,  // Initialized, not yet attempted
    CONN_STATE_CONNECTING  = 1,  // TCP handshake in progress
    CONN_STATE_OPEN        = 2,  // Connection open, ready for data
    CONN_STATE_READING     = 3,  // Reading banner/probe response
    CONN_STATE_CLOSING     = 4,  // Closing connection gracefully
    CONN_STATE_CLOSED      = 5,  // Connection closed
    CONN_STATE_TIMEOUT     = 6,  // Connection timeout
    CONN_STATE_ERROR       = 7,  // Connection error
    CONN_STATE_RESET       = 8   // Connection reset by peer
} conn_state_t;

typedef enum {
    PROBE_NONE     = 0,
    PROBE_TCP_SYN  = 1,
    PROBE_TCP_CON  = 2,
    PROBE_UDP      = 3,
    PROBE_BANNER   = 4,
    PROBE_ICMP_ECHO = 5,   /* ICMP ping */
    PROBE_TCP_ACK   = 6    /* TCP ACK ping for firewall/host discovery */
} probe_type_t;

typedef struct {
    int fd;                          // Socket file descriptor
    conn_state_t state;              // Current connection state
    
    // Timing information
    struct timespec start_time;      // Connection start timestamp
    struct timespec connect_time;    // TCP handshake completion time
    uint64_t rtt_us;                 // Round-trip time in microseconds
    uint32_t timeout_ms;             // Per-connection timeout
    uint32_t elapsed_ms;             // Elapsed time for this connection
    
    // Target information
    struct sockaddr_in addr;         // Target address
    uint16_t port;                   // Target port
    
    // Probe information
    probe_type_t probe_type;         // Current probe type
    
    // Buffer management
    uint8_t *read_buffer;            // Banner/response data
    size_t read_bytes;               // Bytes read
    size_t buffer_capacity;          // Buffer size
    
    // State metadata
    uint32_t retry_count;            // Number of retries
    int error_code;                  // Last system error
    
    // Reference to parent host for context
    void *host_context;              // Opaque host context pointer
} connection_t;

typedef struct {
    // Network engine configuration
    uint32_t max_concurrency_global;   // Max concurrent connections total
    uint32_t max_concurrency_per_host; // Max concurrent per single host
    uint32_t socket_buffer_size;       // Socket buffer size
    uint32_t default_timeout_ms;       // Default timeout per connection
    
    // epoll configuration  
    int epoll_fd;                      // epoll file descriptor
    uint32_t epoll_max_events;         // Max events to retrieve per epoll_wait
    
    // Metrics
    uint64_t total_connections_attempted;
    uint64_t total_connections_successful;
    uint64_t total_timeouts;
    uint64_t total_errors;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} network_engine_t;

/* ====================================================================
   NETWORK ENGINE API
   ==================================================================== */

// Initialize network engine with configuration
network_engine_t* network_engine_init(
    uint32_t max_concurrency_global,
    uint32_t max_concurrency_per_host,
    uint32_t default_timeout_ms
);

// Cleanup and free resources
void network_engine_cleanup(network_engine_t *engine);

// Create a new connection object
connection_t* connection_create(
    const char *ip,
    uint16_t port,
    uint32_t timeout_ms
);

// Free connection object
void connection_free(connection_t *conn);

// Queue connection for processing (returns immediately)
int network_queue_connection(network_engine_t *engine, connection_t *conn);

// Process all pending connections (blocking, returns when finished)
int network_process_batch(
    network_engine_t *engine,
    uint32_t timeout_logic_ms
);

/**
 * After calling network_process_batch(), retrieve list of connections that
 * reached a terminal state (open/closed/error/timeout).  The returned array
 * points to internal storage which remains valid until the next call to this
 * function or until the engine is cleaned up.  The caller **must** free each
 * connection object via connection_free() when finished using it.
 *
 * @param engine network engine instance
 * @param out_list pointer to receive array of connection_t pointers
 * @param out_count pointer to receive number of entries returned
 * @return 0 on success, -1 on error
 */
int network_collect_finished(network_engine_t *engine,
                             connection_t ***out_list,
                             uint32_t *out_count);

// Get connection results
// Returns: CONN_STATE_* on success, -1 on error
conn_state_t connection_get_state(connection_t *conn);
uint64_t connection_get_rtt_us(connection_t *conn);
const uint8_t* connection_get_banner(connection_t *conn, size_t *out_size);
int connection_get_error(connection_t *conn);

// Metrics retrieval
typedef struct {
    uint64_t total_attempted;
    uint64_t total_successful;
    uint64_t total_timeouts;
    uint64_t total_errors;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    float avg_rtt_us;
    float throughput_pps;  // ports per second
} network_metrics_t;

network_metrics_t network_get_metrics(network_engine_t *engine);

#endif // BLACKMAP3_NETWORK_H
