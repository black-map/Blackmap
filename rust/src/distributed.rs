//! Distributed Scanning Architecture
//!
//! Enables scaling BlackMap across multiple machines using a Master-Worker model.
//! Workers connect to the Master and request scan blocks (IP/Port ranges).
//! Results are streamed back asynchronously in JSON format.

use crate::config::ScanConfig;
use crate::scanner::ScanResult;
use crate::error::Result;
use serde::{Deserialize, Serialize};
use tokio::net::{TcpListener, TcpStream};
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::net::SocketAddr;
use std::sync::Arc;
use tokio::sync::Mutex;
use tracing::{info, warn, error};

/// Network messages exchanged between Master and Worker
#[derive(Debug, Serialize, Deserialize)]
pub enum DistributedMessage {
    /// Worker registering to Master
    RegisterWorker { hostname: String, version: String },
    
    /// Master acknowledging registration
    WorkerAccepted { worker_id: u32 },
    
    /// Master sending a block of work
    ScanBlock {
        block_id: u32,
        targets: Vec<String>,
        ports: Vec<u16>,
        timeout_ms: u64,
        stealth_level: u32,
    },
    
    /// Worker returning scan results
    BlockResult {
        worker_id: u32,
        block_id: u32,
        result: ScanResult,
    },
    
    /// Keepalive ping
    Ping,
    
    /// Keepalive pong
    Pong,
}

/// BlackMap Master node
pub struct MasterNode {
    bind_addr: String,
    config: ScanConfig,
    active_workers: Arc<Mutex<Vec<u32>>>,
}

impl MasterNode {
    pub fn new(bind_addr: String, config: ScanConfig) -> Self {
        Self {
            bind_addr,
            config,
            active_workers: Arc::new(Mutex::new(Vec::new())),
        }
    }

    /// Start the Master listener
    pub async fn start(&self) -> Result<()> {
        info!("Starting BlackMap Master Node on {}", self.bind_addr);
        
        let listener = TcpListener::bind(&self.bind_addr).await.map_err(|e: std::io::Error| 
            crate::error::BlackMapError::NetworkError(e.to_string())
        )?;

        loop {
            match listener.accept().await {
                Ok((stream, addr)) => {
                    info!("Worker connected from: {}", addr);
                    let workers = self.active_workers.clone();
                    tokio::spawn(async move {
                        if let Err(e) = Self::handle_worker(stream, addr, workers).await {
                            error!("Worker {} disconnected: {}", addr, e);
                        }
                    });
                }
                Err(e) => {
                    warn!("Failed to accept connection: {}", e);
                }
            }
        }
    }

    async fn handle_worker(mut stream: TcpStream, addr: SocketAddr, _workers: Arc<Mutex<Vec<u32>>>) -> Result<()> {
        let mut buf = vec![0u8; 65535];
        
        loop {
            let n = stream.read(&mut buf).await.map_err(|e: std::io::Error| 
                crate::error::BlackMapError::NetworkError(e.to_string())
            )?;
            
            if n == 0 {
                info!("Worker {} closed connection", addr);
                break;
            }
            
            // Deserialize JSON message
            if let Ok(msg) = serde_json::from_slice::<DistributedMessage>(&buf[..n]) {
                match msg {
                    DistributedMessage::RegisterWorker { hostname, version } => {
                        info!("Registered Worker {} (v{})", hostname, version);
                        let reply = DistributedMessage::WorkerAccepted { worker_id: 1 };
                        let reply_bytes = serde_json::to_vec(&reply).unwrap();
                        stream.write_all(&reply_bytes).await.unwrap();
                    }
                    DistributedMessage::BlockResult { worker_id, block_id, result } => {
                        info!("Received result for block {} from worker {}", block_id, worker_id);
                        // Merge results logic goes here
                        let _ = result;
                    }
                    DistributedMessage::Ping => {
                        let reply = DistributedMessage::Pong;
                        let reply_bytes = serde_json::to_vec(&reply).unwrap();
                        stream.write_all(&reply_bytes).await.unwrap();
                    }
                    _ => {}
                }
            }
        }
        Ok(())
    }
}

/// BlackMap Worker node
pub struct WorkerNode {
    master_addr: String,
}

impl WorkerNode {
    pub fn new(master_addr: String) -> Self {
        Self { master_addr }
    }

    /// Start the Worker and connect to Master
    pub async fn start(&self) -> Result<()> {
        info!("Starting BlackMap Worker, connecting to Master at {}", self.master_addr);
        
        let mut stream = TcpStream::connect(&self.master_addr).await.map_err(|e: std::io::Error| 
            crate::error::BlackMapError::NetworkError(e.to_string())
        )?;

        // Send registration
        let reg = DistributedMessage::RegisterWorker {
            hostname: "worker-node".to_string(), // In reality gethostname()
            version: crate::VERSION.to_string(),
        };
        
        let reg_bytes = serde_json::to_vec(&reg).unwrap();
        stream.write_all(&reg_bytes).await.unwrap();
        
        let mut buf = vec![0u8; 65535];
        loop {
            let n = stream.read(&mut buf).await.map_err(|e: std::io::Error| 
                crate::error::BlackMapError::NetworkError(e.to_string())
            )?;
            
            if n == 0 {
                warn!("Master closed connection");
                break;
            }
            
            if let Ok(msg) = serde_json::from_slice::<DistributedMessage>(&buf[..n]) {
                match msg {
                    DistributedMessage::WorkerAccepted { worker_id } => {
                        info!("Successfully registered with Master! ID: {}", worker_id);
                    }
                    DistributedMessage::ScanBlock { block_id, targets, ports, timeout_ms: _, stealth_level: _ } => {
                        info!("Received Scan Block {} with {} targets and {} ports", block_id, targets.len(), ports.len());
                        // Actual scan dispatch logic would happen here
                    }
                    _ => {}
                }
            }
        }
        
        Ok(())
    }
}
