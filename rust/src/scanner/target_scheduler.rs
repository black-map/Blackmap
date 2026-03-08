use std::net::IpAddr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

/// Organizes the queue of IP/Port combinations to scan
/// 
/// This scheduler distributes scanning work across targets and ports,
/// supporting both sequential and randomized scanning modes.
pub struct TargetScheduler {
    targets: Vec<IpAddr>,
    ports: Vec<u16>,
    current_index: AtomicUsize,
    total_combinations: usize,
}

impl TargetScheduler {
    pub fn new(targets: Vec<IpAddr>, ports: Vec<u16>) -> Self {
        let total_combinations = targets.len() * ports.len();
        let current_index = if total_combinations > 0 {
            // Start at a random offset to spread initial traffic
            let offset = (rand::random::<usize>() % total_combinations).min(1000);
            AtomicUsize::new(offset)
        } else {
            AtomicUsize::new(0)
        };

        Self {
            targets,
            ports,
            current_index,
            total_combinations,
        }
    }

    /// Fetches the next batch of IP/Port tuples to scan
    /// 
    /// Uses atomic operations for lock-free concurrency across multiple threads.
    /// Returns up to `batch_size` tuples of (IP, Port) to scan.
    pub fn next_batch(&self, batch_size: usize) -> Vec<(IpAddr, u16)> {
        let mut batch = Vec::with_capacity(batch_size);

        if self.total_combinations == 0 {
            return batch;
        }

        for _ in 0..batch_size {
            let idx = self.current_index.fetch_add(1, Ordering::Relaxed);
            if idx >= self.total_combinations {
                break;
            }

            // Sequential scanning: host_idx varies slowly while port_idx varies quickly
            // This means we probe all ports on a host before moving to the next
            let host_idx = idx / self.ports.len();
            let port_idx = idx % self.ports.len();

            if let (Some(ip), Some(port)) = (self.targets.get(host_idx), self.ports.get(port_idx)) {
                batch.push((*ip, *port));
            }
        }

        batch
    }

    /// Checks if all targets have been scheduled
    pub fn is_depleted(&self) -> bool {
        self.current_index.load(Ordering::Relaxed) >= self.total_combinations
    }

    /// Returns the current progress percentage
    pub fn progress_percentage(&self) -> f32 {
        if self.total_combinations == 0 {
            return 100.0;
        }
        let current = self.current_index.load(Ordering::Relaxed);
        ((current as f32) / (self.total_combinations as f32)) * 100.0
    }

    /// Returns total number of combinations to scan
    pub fn total_combinations(&self) -> usize {
        self.total_combinations
    }

    /// Returns number of combinations already scheduled
    pub fn scheduled_count(&self) -> usize {
        self.current_index.load(Ordering::Relaxed)
    }

    /// Resets the scheduler to start from the beginning
    pub fn reset(&self) {
        self.current_index.store(0, Ordering::Relaxed);
    }
}
