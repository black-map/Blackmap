//! Scheduling engine for concurrent scanning
//!
//! Manages task scheduling, concurrency limits, and rate limiting

use std::collections::VecDeque;
use parking_lot::Mutex;
use std::sync::Arc;

/// Scheduling task
#[derive(Debug, Clone)]
pub struct Task {
    /// Host index
    pub host_id: usize,

    /// Port to scan
    pub port: u16,

    /// Priority (higher = more urgent)
    pub priority: u32,
}

/// Task scheduler
pub struct Scheduler {
    queue: Arc<Mutex<VecDeque<Task>>>,
    max_concurrent: usize,
    active_tasks: Arc<Mutex<usize>>,
}

impl Scheduler {
    /// Create a new scheduler
    pub fn new(max_concurrent: usize) -> Self {
        Self {
            queue: Arc::new(Mutex::new(VecDeque::new())),
            max_concurrent,
            active_tasks: Arc::new(Mutex::new(0)),
        }
    }

    /// Queue a task
    pub fn queue_task(&self, task: Task) {
        let mut queue = self.queue.lock();
        queue.push_back(task);
    }

    /// Get next task if concurrency allows
    pub fn next_task(&self) -> Option<Task> {
        let mut active = self.active_tasks.lock();
        if *active >= self.max_concurrent {
            return None;
        }

        let mut queue = self.queue.lock();
        let task = queue.pop_front();

        if task.is_some() {
            *active += 1;
        }

        task
    }

    /// Mark task as complete
    pub fn task_complete(&self) {
        let mut active = self.active_tasks.lock();
        if *active > 0 {
            *active -= 1;
        }
    }

    /// Get number of pending tasks
    pub fn pending_count(&self) -> usize {
        self.queue.lock().len()
    }

    /// Get number of active tasks
    pub fn active_count(&self) -> usize {
        *self.active_tasks.lock()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scheduler_concurrency() {
        let sched = Scheduler::new(5);

        for i in 0..10 {
            sched.queue_task(Task {
                host_id: 0,
                port: 80 + i,
                priority: 0,
            });
        }

        // Should only allow 5 concurrent
        for _ in 0..5 {
            assert!(sched.next_task().is_some());
        }
        assert!(sched.next_task().is_none());
    }
}
