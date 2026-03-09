use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct ImapProbe;

impl ServiceProbe for ImapProbe {
    fn name(&self) -> &'static str {
        "imap"
    }

    fn ports(&self) -> Vec<u16> {
        vec![143, 993, 3993]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // IMAP banner grabbing - passive wait for server response
        let mut buffer = [0u8; 512];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let banner = String::from_utf8_lossy(&buffer[..n]);
                
                // IMAP responses start with "* OK " or "* PREAUTH "
                if banner.starts_with("* OK") || banner.starts_with("* PREAUTH") {
                    // Parse IMAP banner, e.g.:
                    // "* OK [CAPABILITY IMAP4rev1 SASL-IR STARTTLS] Dovecot ready."
                    // "* OK Cyrus IMAP4 v1.5.19 server ready"
                    
                    let mut service = "imap".to_string();
                    let mut version: Option<String> = None;
                    let mut confidence = 85u8;
                    
                    if banner.contains("Dovecot") {
                        service = "Dovecot".to_string();
                        // Extract version if present - but Dovecot often doesn't include version in banner
                        // So we'll just use "2.3.10" as a default if no version found
                        if let Some(pos) = banner.find("Dovecot") {
                            let after = &banner[pos..];
                            let tokens: Vec<&str> = after.split_whitespace().collect();
                            if tokens.len() >= 2 && tokens[1].chars().next().unwrap_or('a').is_numeric() {
                                version = Some(tokens[1].to_string());
                            }
                        }
                        confidence = 90;
                    } else if banner.contains("Cyrus") {
                        service = "Cyrus IMAP".to_string();
                        if let Some(pos) = banner.find("Cyrus") {
                            let after = &banner[pos..];
                            if let Some(end) = after.find(|c: char| c == '\n' || c == '\r') {
                                version = Some(after[..end].to_string());
                                confidence = 95;
                            }
                        }
                    } else if banner.contains("Courier") {
                        service = "Courier IMAP".to_string();
                        if let Some(pos) = banner.find("Courier") {
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

pub struct Pop3Probe;

impl ServiceProbe for Pop3Probe {
    fn name(&self) -> &'static str {
        "pop3"
    }

    fn ports(&self) -> Vec<u16> {
        vec![110, 995, 3110]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // POP3 banner grabbing - passive wait for server response
        let mut buffer = [0u8; 512];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                let banner = String::from_utf8_lossy(&buffer[..n]);
                
                // POP3 responses start with "+OK " or "-ERR "
                if banner.starts_with("+OK") {
                    // Parse POP3 banner, e.g.:
                    // "+OK Dovecot pop3d ready."
                    // "+OK QUALCOMM Mails erver"
                    
                    let mut service = "pop3".to_string();
                    let mut version: Option<String> = None;
                    let mut confidence = 85u8;
                    
                    if banner.contains("Dovecot") {
                        service = "Dovecot".to_string();
                        if let Some(pos) = banner.find("Dovecot") {
                            let after = &banner[pos..];
                            let tokens: Vec<&str> = after.split_whitespace().collect();
                            if tokens.len() >= 2 && tokens[1].chars().next().unwrap_or('a').is_numeric() {
                                version = Some(tokens[1].to_string());
                            }
                        }
                        confidence = 90;
                    } else if banner.contains("Courier") {
                        service = "Courier POP3".to_string();
                        if let Some(pos) = banner.find("Courier") {
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
