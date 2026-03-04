#ifndef BLACKMAP3_SCHEDULER_H
#define BLACKMAP3_SCHEDULER_H

#include <stdint.h>
#include <sys/types.h>
#include "network.h"

/* ====================================================================
   SCHEDULER 3.0 - Event-driven task queue
   
   - Efficient task queue (circular buffer)
   - Port ordering strategies
   - Host distribution logic
   - Concurrency management
   - RTT-aware pacing
   ==================================================================== */

typedef enum {
    SCHEDULE_MODE_RANDOM     = 0,  // Random port order
    SCHEDULE_MODE_ASCENDING  = 1,  // Sequential low-to-high
    SCHEDULE_MODE_DESCENDING = 2,  // Sequential high-to-low
    SCHEDULE_MODE_COMMON     = 3   // Common ports first
} schedule_mode_t;

typedef struct {
    uint32_t host_index;    // Which host
    uint16_t port;          // Port number
    probe_type_t probe_type;// Type of probe
} task_t;

typedef struct {
    task_t *queue;                // Circular task queue
    uint32_t queue_capacity;      // Queue size
    uint32_t queue_head;          // Write position
    uint32_t queue_tail;          // Read position
    uint32_t queue_count;         // Current items in queue
    
    schedule_mode_t mode;         // Port ordering strategy
    
    // Concurrency tracking
    uint32_t active_connections;  // Currently active
    uint32_t connections_per_host[256];  // Per-host tracking (max 256 hosts)
    uint32_t num_hosts;
    
    // Metrics
    uint64_t tasks_enqueued;
    uint64_t tasks_completed;
    
    // Configuration
    uint32_t max_concurrency_global;
    uint32_t max_concurrency_per_host;
} scheduler_t;

typedef struct {
    uint32_t **ports;           // ports[host_idx] = array of ports
    uint16_t *port_counts;      // port_counts[host_idx] = number of ports
    char **target_ips;          // target_ips[host_idx] = IP string
    uint32_t num_hosts;
    uint32_t num_total_tasks;   // Total tasks to schedule
} scan_plan_t;

/* ====================================================================
   SCHEDULER API
   ==================================================================== */

// Create scheduler
scheduler_t* scheduler_create(
    uint32_t max_concurrency_global,
    uint32_t max_concurrency_per_host,
    schedule_mode_t mode
);

// Free scheduler
void scheduler_free(scheduler_t *sched);

// Queue tasks into scheduler from scan plan
// Returns: number of tasks enqueued, -1 on error
int scheduler_enqueue_plan(scheduler_t *sched, const scan_plan_t *plan);

// Dequeue next task (respecting concurrency limits)
// Returns: task_t on success, NULL if no tasks available or limits reached
task_t* scheduler_next_task(scheduler_t *sched);

// Notify scheduler that task is complete
void scheduler_mark_complete(scheduler_t *sched, uint32_t host_index);

// Check if all tasks completed
int scheduler_is_finished(scheduler_t *sched);

// Get number of tasks still pending
uint32_t scheduler_pending_count(scheduler_t *sched);

// Get active connection count
uint32_t scheduler_active_count(scheduler_t *sched);

// For per-host concurrency limiting
// Returns: 1 if can connect to this host, 0 if limit reached
int scheduler_can_connect_to_host(scheduler_t *sched, uint32_t host_index);

// Scheduler metrics
typedef struct {
    uint32_t queue_size;
    uint32_t queue_used;
    uint32_t active_connections;
    uint64_t tasks_completed;
    uint64_t tasks_pending;
} scheduler_metrics_t;

scheduler_metrics_t scheduler_get_metrics(scheduler_t *sched);

#endif // BLACKMAP3_SCHEDULER_H
