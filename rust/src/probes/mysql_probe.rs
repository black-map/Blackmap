use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct MysqlProbe;

impl ServiceProbe for MysqlProbe {
    fn name(&self) -> &'static str {
        "mysql"
    }

    fn ports(&self) -> Vec<u16> {
        vec![3306]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // MySQL handshake packet
        let mut buffer = [0u8; 128];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n >= 4 => {
                // MySQL initial handshake starts with packet length (3 bytes) + sequence (1 byte)
                // Then protocol version (1 byte), version string, etc.
                
                // Check if it looks like MySQL handshake
                if n > 4 && buffer[3] == 0 { // Sequence number 0
                    // Protocol version should be 10 for MySQL 4.0+
                    if buffer[4] == 10 {
                        // Version string starts at offset 5
                        let version_start = 5;
                        let mut version_end = version_start;
                        while version_end < n && buffer[version_end] != 0 {
                            version_end += 1;
                        }
                        
                        if version_end > version_start {
                            let version_str = String::from_utf8_lossy(&buffer[version_start..version_end]);
                            return Some(ServiceInfo {
                                service: "mysql".to_string(),
                                version: Some(format!("MySQL {}", version_str)),
                                confidence: 95,
                            });
                        }
                    }
                }
            }
            _ => {}
        }
        
        None
    }
}