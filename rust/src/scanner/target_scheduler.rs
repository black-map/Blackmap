use std::net::IpAddr;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;

/// Organizes the queue of IP/Port combinations to scan.
pub struct TargetScheduler {
    targets: Vec<IpAddr>,
    ports: Vec<u16>,
    current_index: AtomicUsize,
    total_combinations: usize,
}

impl TargetScheduler {
    pub fn new(targets: Vec<IpAddr>, ports: Vec<u16>) -> Self {
        let total_combinations = targets.len() * ports.len();
        Self {
            targets,
            ports,
            current_index: AtomicUsize::new(0),
            total_combinations,
        }
    }

    /// Fetches the next batch of IP/Port tuples to scan.
    /// Uses monotonic atomic increments for lock-free concurrency.
    pub fn next_batch(&self, batch_size: usize) -> Vec<(IpAddr, u16)> {
        let mut batch = Vec::with_capacity(batch_size);
        
        for _ in 0..batch_size {
            let idx = self.current_index.fetch_add(1, Ordering::Relaxed);
            if idx >= self.total_combinations {
                break;
            }
            
            // To spread load and evade naive basic detection, 
            // you could randomize this index mapping.
            // But for high-speed sequential scanning, standard striding works.
            
            let host_idx = idx / self.ports.len();
            let port_idx = idx % self.ports.len();
            
            if let (Some(ip), Some(port)) = (self.targets.get(host_idx), self.ports.get(port_idx)) {
                batch.push((*ip, *port));
            }
        }
        
        batch
    }
    
    pub fn is_depleted(&self) -> bool {
        self.current_index.load(Ordering::Relaxed) >= self.total_combinations
    }

    pub fn reset(&self) {
        self.current_index.store(0, Ordering::Relaxed);
    }
}
