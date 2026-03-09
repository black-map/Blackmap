use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct SmtpProbe;

impl ServiceProbe for SmtpProbe {
    fn name(&self) -> &'static str {
        "smtp"
    }

    fn ports(&self) -> Vec<u16> {
        vec![25, 26, 2525, 587, 465]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // SMTP banner grabbing - passive wait for server response
        let mut buffer = [0u8; 512];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let banner = String::from_utf8_lossy(&buffer[..n]);
                
                // SMTP responses start with "220 "
                if banner.starts_with("220") {
                    // Parse SMTP banner, e.g.:
                    // "220 mail.example.com ESMTP Postfix"
                    // "220 mail.example.com Exim 4.99.1"
                    // "220 mail.example.com Microsoft ESMTP MAIL Service"
                    
                    let mut service = "smtp".to_string();
                    let mut version: Option<String> = None;
                    let mut confidence = 80u8;
                    
                    if banner.contains("Exim") {
                        service = "Exim".to_string();
                        // Extract version, e.g., "Exim 4.99.1" from "Exim 4.99.1 #2 Mon..."
                        if let Some(pos) = banner.find("Exim") {
                            let after = &banner[pos..];
                            // Split by whitespace and get tokens: ["Exim", "4.99.1", "#2", ...]
                            let tokens: Vec<&str> = after.split_whitespace().collect();
                            if tokens.len() >= 2 {
                                // tokens[0] = "Exim", tokens[1] = "4.99.1"
                                let version_str = tokens[1];
                                version = Some(version_str.to_string());
                                confidence = 95;
                            }
                        }
                    } else if banner.contains("Postfix") {
                        service = "Postfix".to_string();
                        if let Some(pos) = banner.find("Postfix") {
                            let after = &banner[pos..];
                            if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                version = Some(after[..end].to_string());
                                confidence = 95;
                            }
                        }
                    } else if banner.contains("Sendmail") {
                        service = "Sendmail".to_string();
                        if let Some(pos) = banner.find("Sendmail") {
                            let after = &banner[pos..];
                            if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                version = Some(after[..end].to_string());
                                confidence = 95;
                            }
                        }
                    } else if banner.contains("Microsoft") || banner.contains("ESMTP") {
                        service = "Microsoft Exchange".to_string();
                        if let Some(pos) = banner.find("Microsoft") {
                            let after = &banner[pos..];
                            if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                version = Some(after[..end].to_string());
                                confidence = 90;
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
