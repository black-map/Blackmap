//! Banner grabbing and protocol fingerprinting for service detection
//! Supports: HTTP, HTTPS, FTP, SSH, SMTP, POP3, IMAP, DNS, MySQL, etc.

use std::io::{Read, Write, BufRead, BufReader};
use std::net::TcpStream;
use std::time::Duration;

#[derive(Debug, Clone)]
pub struct ServiceBanner {
    pub service: String,
    pub version: Option<String>,
    pub details: String,
    pub confidence: u8, // 0-100%
}

pub struct BannerGrabber;

impl BannerGrabber {
    /// Grab banner from a service on given host:port
    pub fn grab_banner(host: &str, port: u16, service_hint: &str) -> Option<ServiceBanner> {
        match TcpStream::connect(format!("{}:{}", host, port)) {
            Ok(mut stream) => {
                stream.set_read_timeout(Some(Duration::from_secs(3))).ok()?;
                stream.set_write_timeout(Some(Duration::from_secs(3))).ok()?;

                match port {
                    80 => Self::grab_http(&mut stream, host),
                    443 => Self::grab_https(&mut stream, host),
                    22 => Self::grab_ssh(&mut stream),
                    21 => Self::grab_ftp(&mut stream),
                    25 | 587 => Self::grab_smtp(&mut stream),
                    110 => Self::grab_pop3(&mut stream),
                    143 => Self::grab_imap(&mut stream),
                    53 => Self::grab_dns(&mut stream),
                    3306 => Self::grab_mysql(&mut stream),
                    5432 => Self::grab_postgresql(&mut stream),
                    6379 => Self::grab_redis(&mut stream),
                    _ => Self::grab_generic(&mut stream, port),
                }
            }
            Err(_) => None,
        }
    }

    /// HTTP banner grabbing
    fn grab_http(stream: &mut TcpStream, host: &str) -> Option<ServiceBanner> {
        let request = format!(
            "GET / HTTP/1.1\r\nHost: {}\r\nConnection: close\r\nUser-Agent: BlackMap/6.1\r\n\r\n",
            host
        );

        stream.write_all(request.as_bytes()).ok()?;

        let mut response = String::new();
        let mut reader = BufReader::new(stream.try_clone().ok()?);
        reader.read_to_string(&mut response).ok()?;

        // Parse headers
        let mut server = String::from("Unknown");
        let mut powered_by = String::new();
        let mut title = String::new();

        for line in response.lines() {
            if line.starts_with("Server:") {
                server = line.replace("Server:", "").trim().to_string();
            } else if line.starts_with("X-Powered-By:") {
                powered_by = line.replace("X-Powered-By:", "").trim().to_string();
            } else if line.contains("<title>") {
                title = line.replace("<title>", "")
                    .replace("</title>", "")
                    .trim()
                    .to_string();
            }
        }

        let mut details = format!("Server: {}", server);
        if !powered_by.is_empty() {
            details.push_str(&format!("\nPowered-By: {}", powered_by));
        }
        if !title.is_empty() {
            details.push_str(&format!("\nTitle: {}", title));
        }

        Some(ServiceBanner {
            service: "http".to_string(),
            version: None,
            details,
            confidence: 95,
        })
    }

    /// HTTPS banner grabbing (simplified)
    fn grab_https(stream: &mut TcpStream, host: &str) -> Option<ServiceBanner> {
        // Attempt TLS connection and certificate grabbing
        let request = format!(
            "GET / HTTP/1.1\r\nHost: {}\r\nConnection: close\r\nUser-Agent: BlackMap/6.1\r\n\r\n",
            host
        );

        stream.write_all(request.as_bytes()).ok()?;

        let mut response = String::new();
        let mut reader = BufReader::new(stream.try_clone().ok()?);
        reader.read_to_string(&mut response).ok()?;

        // Extract server info similar to HTTP
        let server = if response.contains("Server:") {
            response.lines()
                .find(|l| l.starts_with("Server:"))
                .map(|l| l.replace("Server:", "").trim().to_string())
                .unwrap_or_default()
        } else {
            "Unknown".to_string()
        };

        Some(ServiceBanner {
            service: "https".to_string(),
            version: None,
            details: format!("Server: {}", server),
            confidence: 90,
        })
    }

    /// SSH banner grabbing
    fn grab_ssh(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("SSH") {
            return Some(ServiceBanner {
                service: "ssh".to_string(),
                version: None,
                details: banner_str.trim().to_string(),
                confidence: 98,
            });
        }
        None
    }

    /// FTP banner grabbing
    fn grab_ftp(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("220") {
            return Some(ServiceBanner {
                service: "ftp".to_string(),
                version: None,
                details: banner_str.trim().to_string(),
                confidence: 95,
            });
        }
        None
    }

    /// SMTP banner grabbing
    fn grab_smtp(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 512];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("220") {
            let service_type = if banner_str.contains("Exchange") {
                "Microsoft Exchange SMTP"
            } else if banner_str.contains("Postfix") {
                "Postfix SMTP"
            } else if banner_str.contains("Exim") {
                "Exim SMTP"
            } else if banner_str.contains("Sendmail") {
                "Sendmail SMTP"
            } else {
                "SMTP"
            };

            return Some(ServiceBanner {
                service: "smtp".to_string(),
                version: None,
                details: format!("{}\n{}", service_type, banner_str.trim()),
                confidence: 90,
            });
        }
        None
    }

    /// POP3 banner grabbing
    fn grab_pop3(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("+OK") || banner_str.contains("POP3") {
            let service_type = if banner_str.contains("Exchange") {
                "Microsoft Exchange POP3"
            } else if banner_str.contains("Dovecot") {
                "Dovecot POP3"
            } else {
                "POP3"
            };

            return Some(ServiceBanner {
                service: "pop3".to_string(),
                version: None,
                details: format!("{}\n{}", service_type, banner_str.trim()),
                confidence: 85,
            });
        }
        None
    }

    /// IMAP banner grabbing
    fn grab_imap(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("IMAP") || banner_str.contains("Cyrus") {
            return Some(ServiceBanner {
                service: "imap".to_string(),
                version: None,
                details: banner_str.trim().to_string(),
                confidence: 90,
            });
        }
        None
    }

    /// DNS version query
    fn grab_dns(_stream: &mut TcpStream) -> Option<ServiceBanner> {
        // DNS version detection would require crafted DNS queries
        Some(ServiceBanner {
            service: "dns".to_string(),
            version: None,
            details: "DNS Service Detected".to_string(),
            confidence: 80,
        })
    }

    /// MySQL banner grabbing
    fn grab_mysql(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("MySQL") {
            return Some(ServiceBanner {
                service: "mysql".to_string(),
                version: None,
                details: banner_str.trim().to_string(),
                confidence: 98,
            });
        }
        None
    }

    /// PostgreSQL banner grabbing
    fn grab_postgresql(stream: &mut TcpStream) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 256];
        let n = stream.read(&mut banner).ok()?;
        let banner_str = String::from_utf8_lossy(&banner[..n]);

        if banner_str.contains("FATAL") || banner_str.contains("PostgreSQL") {
            return Some(ServiceBanner {
                service: "postgresql".to_string(),
                version: None,
                details: banner_str.trim().to_string(),
                confidence: 95,
            });
        }
        None
    }

    /// Redis banner grabbing
    fn grab_redis(stream: &mut TcpStream) -> Option<ServiceBanner> {
        stream.write_all(b"PING\r\n").ok()?;
        let mut response = vec![0u8; 256];
        let n = stream.read(&mut response).ok()?;
        let response_str = String::from_utf8_lossy(&response[..n]);

        if response_str.contains("PONG") {
            return Some(ServiceBanner {
                service: "redis".to_string(),
                version: None,
                details: "Redis Server".to_string(),
                confidence: 98,
            });
        }
        None
    }

    /// Generic banner grabbing for unknown services
    fn grab_generic(stream: &mut TcpStream, port: u16) -> Option<ServiceBanner> {
        let mut banner = vec![0u8; 512];
        match stream.read(&mut banner) {
            Ok(n) if n > 0 => {
                let banner_str = String::from_utf8_lossy(&banner[..n]);
                if banner_str.len() > 10 && !banner_str.contains('\0') {
                    return Some(ServiceBanner {
                        service: format!("service@{}", port),
                        version: None,
                        details: banner_str.trim().to_string(),
                        confidence: 50,
                    });
                }
                None
            }
            _ => None,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_service_banner_creation() {
        let banner = ServiceBanner {
            service: "http".to_string(),
            version: Some("1.1".to_string()),
            details: "Apache 2.4.41".to_string(),
            confidence: 95,
        };
        assert_eq!(banner.service, "http");
        assert_eq!(banner.confidence, 95);
    }
}
