use std::net::IpAddr;
use tokio::sync::mpsc;
use crossbeam::queue::SegQueue;
use std::sync::Arc;
use tracing::{info, warn};

pub struct StatelessScanner {
    pub targets: Vec<IpAddr>,
    pub ports: Vec<u16>,
    pub rate_limit: u32,
    pub source_ip: Option<IpAddr>,
}

#[derive(Debug)]
pub struct OpenPort {
    pub ip: IpAddr,
    pub port: u16,
}

impl StatelessScanner {
    pub fn new(targets: Vec<IpAddr>, ports: Vec<u16>, rate_limit: u32) -> Self {
        Self {
            targets,
            ports,
            rate_limit,
            source_ip: None,
        }
    }

    /// Primary execution handler that spins up the stateless masscan-style network threads
    pub async fn run(&self) -> Result<Vec<OpenPort>, String> {
        info!("Initializing Stateless TCP-SYN Scanner...");
        
        // Use a lock-free queue to store discovered open ports rapidly from the receiver thread
        let results_queue = Arc::new(SegQueue::new());
        let results_clone = results_queue.clone();
        
        // Set up signaling channel to notify the receiver when sending is done
        let (tx, mut rx) = mpsc::channel(1);

        // --- Receiver Thread (Captures SYN-ACK independently) ---
        let receiver_task = tokio::spawn(async move {
            crate::receiver::start_listening(results_clone, &mut rx).await
        });

        // --- Sender Thread (Blasts SYN packets state-lessly) ---
        let targets = self.targets.clone();
        let ports = self.ports.clone();
        let rate_limit = self.rate_limit;
        
        crate::sender::start_sending(&targets, &ports, rate_limit).await?;
        
        // Notify receiver that we are done sending, but wait slightly for late packets
        tokio::time::sleep(tokio::time::Duration::from_millis(1500)).await;
        let _ = tx.send(()).await;
        
        // Await receiver shutdown
        let _ = receiver_task.await;

        let mut discovered = Vec::new();
        while let Some(port) = results_queue.pop() {
            discovered.push(port);
        }

        Ok(discovered)
    }
}
