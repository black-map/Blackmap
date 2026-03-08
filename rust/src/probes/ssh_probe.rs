use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct SshProbe;

impl ServiceProbe for SshProbe {
    fn name(&self) -> &'static str {
        "ssh"
    }

    fn ports(&self) -> Vec<u16> {
        vec![22]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // SSH banner grabbing
        let mut buffer = [0u8; 256];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let banner = String::from_utf8_lossy(&buffer[..n]);
                
                // SSH banners start with "SSH-"
                if banner.starts_with("SSH-") {
                    // Parse version, e.g., "SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.1"
                    let parts: Vec<&str> = banner.split_whitespace().collect();
                    if parts.len() >= 2 {
                        let version_part = parts[1]; // "2.0-OpenSSH_8.9p1"
                        let version = if version_part.contains("OpenSSH") {
                            Some(format!("OpenSSH {}", version_part.split('_').nth(1).unwrap_or("unknown")))
                        } else {
                            Some(version_part.to_string())
                        };
                        
                        return Some(ServiceInfo {
                            service: "ssh".to_string(),
                            version,
                            confidence: 90,
                        });
                    }
                }
            }
            _ => {}
        }
        
        None
    }
}