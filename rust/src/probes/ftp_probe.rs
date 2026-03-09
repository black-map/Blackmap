use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct FtpProbe;

impl ServiceProbe for FtpProbe {
    fn name(&self) -> &'static str {
        "ftp"
    }

    fn ports(&self) -> Vec<u16> {
        vec![21, 2121, 3121]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // FTP banner grabbing - passive wait for server response
        let mut buffer = [0u8; 512];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let banner = String::from_utf8_lossy(&buffer[..n]);
                
                // FTP responses start with "220 "
                if banner.starts_with("220") {
                    // Parse FTP banner, e.g.:
                    // "220 (vsFTPd 3.0.3)"
                    // "220 Welcome to Pure-FTPd"
                    // "220 FTP server ready"
                    
                    let mut service = "ftp".to_string();
                    let mut version: Option<String> = None;
                    let mut confidence = 85u8;
                    
                    if banner.contains("Pure-FTPd") {
                        service = "Pure-FTPd".to_string();
                        // Extract version if present, e.g., "Pure-FTPd 1.0.50"
                        if let Some(pos) = banner.find("Pure-FTPd") {
                            let after = &banner[pos..];
                            let tokens: Vec<&str> = after.split_whitespace().collect();
                            if tokens.len() >= 2 && tokens[1].chars().next().unwrap_or('a').is_numeric() {
                                version = Some(tokens[1].to_string());
                                confidence = 95;
                            } else {
                                // If version not found, include the banner portion
                                if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                    let banner_part = &after[..end.min(50)];
                                    version = Some(banner_part.to_string());
                                    confidence = 85;
                                }
                            }
                        }
                    } else if banner.contains("vsFTPd") {
                        service = "vsFTPd".to_string();
                        // Extract version, e.g., "(vsFTPd 3.0.3)"
                        if let Some(start) = banner.find('(') {
                            if let Some(end) = banner[start..].find(')') {
                                let version_str = banner[start+1..start+end].to_string();
                                version = Some(version_str.to_string());
                                confidence = 95;
                            }
                        }
                    } else if banner.contains("ProFTPD") {
                        service = "ProFTPD".to_string();
                        if let Some(pos) = banner.find("ProFTPD") {
                            let after = &banner[pos..];
                            if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                version = Some(after[..end].to_string());
                                confidence = 95;
                            }
                        }
                    }
                    
                    return Some(ServiceInfo {
                        service,
                        version,
                        confidence,
                    });
                }
            }
            _ => {}
        }
        
        None
    }
}
