/* ====================================================================
   BLACKMAP 3.0 - SCHEDULER IMPLEMENTATION
   
   Event-driven task queue with concurrency control
   ===================================================================== */

#include "blackmap3/scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

scheduler_t* scheduler_create(
    uint32_t max_concurrency_global,
    uint32_t max_concurrency_per_host,
    schedule_mode_t mode)
{
    scheduler_t *sched = (scheduler_t*)malloc(sizeof(*sched));
    if (!sched) return NULL;
    
    memset(sched, 0, sizeof(*sched));
    
    sched->queue_capacity = 4096;  // Can hold 4K tasks
    sched->queue = (task_t*)malloc(sizeof(task_t) * sched->queue_capacity);
    
    if (!sched->queue) {
        free(sched);
        return NULL;
    }
    
    sched->max_concurrency_global = max_concurrency_global;
    sched->max_concurrency_per_host = max_concurrency_per_host;
    sched->mode = mode;
    sched->active_connections = 0;
    sched->num_hosts = 0;
    
    return sched;
}

void scheduler_free(scheduler_t *sched) {
    if (!sched) return;
    if (sched->queue) free(sched->queue);
    free(sched);
}

int scheduler_enqueue_plan(scheduler_t *sched, const scan_plan_t *plan) {
    if (!sched || !plan) return -1;
    
    uint32_t enqueued = 0;
    
    sched->num_hosts = plan->num_hosts;
    memset(sched->connections_per_host, 0, sizeof(sched->connections_per_host));
    
    // Enqueue all port tasks for each host
    for (uint32_t h = 0; h < plan->num_hosts; h++) {
        for (uint16_t p = 0; p < plan->port_counts[h]; p++) {
            if (sched->queue_count >= sched->queue_capacity) {
                return -1;  // Queue overflow
            }
            
            uint32_t idx = sched->queue_head;
            sched->queue[idx].host_index = h;
            sched->queue[idx].port = plan->ports[h][p];
            sched->queue[idx].probe_type = PROBE_TCP_CON;
            
            sched->queue_head = (sched->queue_head + 1) % sched->queue_capacity;
            sched->queue_count++;
            enqueued++;
        }
    }
    
    sched->tasks_enqueued = enqueued;
    return enqueued;
}

task_t* scheduler_next_task(scheduler_t *sched) {
    if (!sched || sched->queue_count == 0) {
        return NULL;
    }
    
    // Check global concurrency limit
    if (sched->active_connections >= sched->max_concurrency_global) {
        return NULL;
    }
    
    // Peek at next task
    task_t *task = &sched->queue[sched->queue_tail];
    
    // Check per-host concurrency limit
    if (sched->connections_per_host[task->host_index] >= sched->max_concurrency_per_host) {
        // This host is at limit, try to find another task
        // For now, just return NULL (could implement round-robin)
        return NULL;
    }
    
    // Dequeue task
    sched->queue_tail = (sched->queue_tail + 1) % sched->queue_capacity;
    sched->queue_count--;
    
    // Track concurrency
    sched->connections_per_host[task->host_index]++;
    sched->active_connections++;
    
    return task;
}

void scheduler_mark_complete(scheduler_t *sched, uint32_t host_index) {
    if (!sched || host_index >= sched->num_hosts) return;
    
    if (sched->connections_per_host[host_index] > 0) {
        sched->connections_per_host[host_index]--;
    }
    
    if (sched->active_connections > 0) {
        sched->active_connections--;
    }
    
    sched->tasks_completed++;
}

int scheduler_is_finished(scheduler_t *sched) {
    if (!sched) return 1;
    return sched->queue_count == 0 && sched->active_connections == 0;
}

uint32_t scheduler_pending_count(scheduler_t *sched) {
    if (!sched) return 0;
    return sched->queue_count;
}

uint32_t scheduler_active_count(scheduler_t *sched) {
    if (!sched) return 0;
    return sched->active_connections;
}

int scheduler_can_connect_to_host(scheduler_t *sched, uint32_t host_index) {
    if (!sched || host_index >= sched->num_hosts) return 0;
    
    // Check global limit
    if (sched->active_connections >= sched->max_concurrency_global) {
        return 0;
    }
    
    // Check per-host limit
    if (sched->connections_per_host[host_index] >= sched->max_concurrency_per_host) {
        return 0;
    }
    
    return 1;
}

scheduler_metrics_t scheduler_get_metrics(scheduler_t *sched) {
    scheduler_metrics_t m = {0};
    
    if (!sched) return m;
    
    m.queue_size = sched->queue_capacity;
    m.queue_used = sched->queue_count;
    m.active_connections = sched->active_connections;
    m.tasks_completed = sched->tasks_completed;
    m.tasks_pending = sched->queue_count;
    
    return m;
}
