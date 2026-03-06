//! Banner Grabbing module
//!
//! Extracts banners from open ports (SSH, HTTP, FTP, etc).

use std::net::SocketAddr;
use std::time::Duration;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::TcpStream;
use tokio::time::timeout;

/// Represents a grabbed banner
#[derive(Debug, Clone)]
pub struct Banner {
    pub port: u16,
    pub payload: String,
}

/// Grabs a banner from an open port
pub async fn grab_banner(ip: &std::net::IpAddr, port: u16) -> Option<Banner> {
    let addr = SocketAddr::new(*ip, port);
    let connect_timeout = Duration::from_secs(2);
    let read_timeout = Duration::from_millis(1500);

    // Attempt connection
    let mut stream = match timeout(connect_timeout, TcpStream::connect(addr)).await {
        Ok(Ok(s)) => s,
        _ => return None,
    };

    let mut buffer = [0; 1024];

    // 1. Try to read immediately (good for SSH, FTP, SMTP that send a banner on connect)
    match timeout(read_timeout, stream.read(&mut buffer)).await {
        Ok(Ok(n)) if n > 0 => {
            let payload = String::from_utf8_lossy(&buffer[..n]).to_string().trim().to_string();
            return Some(Banner { port, payload });
        }
        _ => {} // No immediate banner, proceed
    };

    // 2. If nothing received, try sending a generic HTTP GET request (useful for Web, REST APIs)
    let http_request = format!("GET / HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", ip);
    if timeout(Duration::from_millis(500), stream.write_all(http_request.as_bytes())).await.is_err() {
        return None; 
    }

    match timeout(read_timeout, stream.read(&mut buffer)).await {
        Ok(Ok(n)) if n > 0 => {
            let payload = String::from_utf8_lossy(&buffer[..n]).to_string()
                .lines()
                .take(3) // Take max 3 lines to avoid grabbing entire HTML
                .collect::<Vec<_>>()
                .join("\n")
                .trim()
                .to_string();
            return Some(Banner { port, payload });
        }
        _ => {}
    };

    None
}
