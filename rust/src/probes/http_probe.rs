use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;
use regex::Regex;

pub struct HttpProbe;

impl ServiceProbe for HttpProbe {
    fn name(&self) -> &'static str {
        "http"
    }

    fn ports(&self) -> Vec<u16> {
        vec![80, 8080, 8000, 8888]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        let request = b"GET / HTTP/1.0\r\nHost: localhost\r\nUser-Agent: BlackMap/1.4\r\nAccept: */*\r\n\r\n";
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(request)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 4096];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                parse_http_response(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses an HTTP response to extract the service version and optionally the title.
pub fn parse_http_response(buffer: &[u8]) -> Option<ServiceInfo> {
    let response = String::from_utf8_lossy(buffer);
    
    // Valid HTTP responses should start with HTTP/
    if !response.starts_with("HTTP/") {
        return None;
    }

    let mut version = None;
    let mut title = None;

    // Split headers and body
    let parts: Vec<&str> = response.splitn(2, "\r\n\r\n").collect();
    let headers = parts.get(0).unwrap_or(&"");
    let body = parts.get(1).unwrap_or(&"");

    // Extract Server header
    for line in headers.lines() {
        if line.to_lowercase().starts_with("server:") {
            let server_val = line[7..].trim();
            version = parse_server_header(server_val);
            break;
        }
    }

    // Extract Title from body
    if let Some(t) = extract_html_title(body) {
        title = Some(t);
    }

    // Compose final version string
    let mut final_version = version;
    if let Some(t) = title {
        if let Some(v) = final_version {
            final_version = Some(format!("{} (Title: {})", v, t));
        } else {
            final_version = Some(format!("(Title: {})", t));
        }
    }

    Some(ServiceInfo {
        service: "http".to_string(),
        version: final_version,
        confidence: 85,
    })
}

fn parse_server_header(server: &str) -> Option<String> {
    let lower = server.to_lowercase();
    if lower.contains("nginx") {
        if let Some(start) = lower.find("nginx/") {
            let ver = &server[start + 6..];
            Some(format!("nginx {}", ver.split_whitespace().next().unwrap_or(ver)))
        } else {
            Some("nginx".to_string())
        }
    } else if lower.contains("apache") {
        if let Some(start) = lower.find("apache/") {
            let ver = &server[start + 7..];
            Some(format!("Apache {}", ver.split_whitespace().next().unwrap_or(ver)))
        } else {
            Some("Apache".to_string())
        }
    } else if lower.contains("microsoft-iis") {
        if let Some(start) = lower.find("microsoft-iis/") {
            let ver = &server[start + 14..];
            Some(format!("IIS {}", ver.split_whitespace().next().unwrap_or(ver)))
        } else {
            Some("Microsoft IIS".to_string())
        }
    } else if lower.contains("lighttpd") {
        if let Some(start) = lower.find("lighttpd/") {
            let ver = &server[start + 9..];
            Some(format!("lighttpd {}", ver.split_whitespace().next().unwrap_or(ver)))
        } else {
            Some("lighttpd".to_string())
        }
    } else {
        Some(server.to_string())
    }
}

fn extract_html_title(body: &str) -> Option<String> {
    // Simple regex to extract <title> tags (case-insensitive)
    let re = Regex::new(r"(?i)<title[^>]*>(.*?)</title>").ok()?;
    if let Some(caps) = re.captures(body) {
        if let Some(m) = caps.get(1) {
            let title = m.as_str().trim();
            // Truncate if too long to keep the version string reasonable
            if title.len() > 50 {
                return Some(format!("{}...", &title[..47]));
            }
            return Some(title.to_string());
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_http_nginx() {
        let response = b"HTTP/1.1 200 OK\r\nServer: nginx/1.18.0 (Ubuntu)\r\nContent-Type: text/html\r\n\r\n<html><head><title>Welcome to Nginx!</title></head></html>";
        let info = parse_http_response(response).unwrap();
        assert_eq!(info.service, "http");
        assert_eq!(info.version.unwrap(), "nginx 1.18.0 (Title: Welcome to Nginx!)");
    }

    #[test]
    fn test_parse_http_apache() {
        let response = b"HTTP/1.1 403 Forbidden\r\nDate: Mon, 12 Oct 2020 12:00:00 GMT\r\nServer: Apache/2.4.41 (Ubuntu)\r\n\r\n";
        let info = parse_http_response(response).unwrap();
        assert_eq!(info.version.unwrap(), "Apache 2.4.41");
    }

    #[test]
    fn test_parse_http_no_server_header() {
        let response = b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<title>Embedded Device</title>";
        let info = parse_http_response(response).unwrap();
        assert_eq!(info.version.unwrap(), "(Title: Embedded Device)");
    }

    #[test]
    fn test_parse_http_invalid() {
        let response = b"SSH-2.0-OpenSSH_8.9p1";
        assert!(parse_http_response(response).is_none());
    }
}