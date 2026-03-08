use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct RedisProbe;

impl ServiceProbe for RedisProbe {
    fn name(&self) -> &'static str {
        "redis"
    }

    fn ports(&self) -> Vec<u16> {
        vec![6379]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // Redis responds to PING with PONG
        let ping_cmd = b"PING\r\n";
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(ping_cmd)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 128];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let response = String::from_utf8_lossy(&buffer[..n]);
                
                if response.starts_with("+PONG") {
                    // Try to get version with INFO command
                    let info_cmd = b"INFO\r\n";
                    if tokio::time::timeout(Duration::from_secs(5), stream.write_all(info_cmd)).await.is_ok() {
                        let mut info_buffer = [0u8; 1024];
                        if let Ok(Ok(m)) = tokio::time::timeout(Duration::from_secs(5), stream.read(&mut info_buffer)).await {
                            let info_response = String::from_utf8_lossy(&info_buffer[..m]);
                            
                            // Parse redis_version from INFO
                            for line in info_response.lines() {
                                if line.starts_with("redis_version:") {
                                    let version = line[14..].trim();
                                    return Some(ServiceInfo {
                                        service: "redis".to_string(),
                                        version: Some(format!("Redis {}", version)),
                                        confidence: 95,
                                    });
                                }
                            }
                        }
                    }
                    
                    // If INFO fails, just confirm Redis
                    return Some(ServiceInfo {
                        service: "redis".to_string(),
                        version: None,
                        confidence: 85,
                    });
                }
            }
            _ => {}
        }
        
        None
    }
}