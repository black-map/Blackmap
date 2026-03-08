use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct DockerProbe;

impl ServiceProbe for DockerProbe {
    fn name(&self) -> &'static str {
        "docker"
    }

    fn ports(&self) -> Vec<u16> {
        vec![2375, 2376] // 2375 HTTP, 2376 HTTPS
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        let version_request = b"GET /version HTTP/1.1\r\nHost: localhost\r\nUser-Agent: BlackMap/1.4\r\n\r\n";
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(version_request)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 4096];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                parse_docker_version_response(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses the Docker API /version JSON response to extract version strings
pub fn parse_docker_version_response(buffer: &[u8]) -> Option<ServiceInfo> {
    let response = String::from_utf8_lossy(buffer);
    
    // We expect a 200 OK and "application/json" for Docker
    if !response.contains("HTTP/1.") || !response.contains("200 OK") {
        return None;
    }

    // Split headers and body to focus on JSON
    let parts: Vec<&str> = response.splitn(2, "\r\n\r\n").collect();
    let body = parts.get(1).unwrap_or(&"");

    if body.contains("\"Version\":") && body.contains("\"Os\":") {
        let version = extract_json_str_value(body, "\"Version\"");
        let os = extract_json_str_value(body, "\"Os\"");
        let arch = extract_json_str_value(body, "\"Arch\"");

        let mut final_version = None;
        if let Some(v) = version {
            let mut v_str = format!("Docker Engine {}", v);
            if let Some(o) = os {
                if let Some(a) = arch {
                    v_str.push_str(&format!(" ({}/{})", o, a));
                } else {
                    v_str.push_str(&format!(" ({})", o));
                }
            }
            final_version = Some(v_str);
        }

        return Some(ServiceInfo {
            service: "docker".to_string(),
            version: final_version,
            confidence: 95,
        });
    }

    // If we got 200 OK but couldn't parse version (maybe we pinged something else?), we at least check if "docker" is in the response anywhere
    if response.to_lowercase().contains("docker") {
        return Some(ServiceInfo {
            service: "docker".to_string(),
            version: None,
            confidence: 80,
        });
    }

    None
}

/// Quick and dirty JSON string field extractor (avoids adding serde_json as a direct dependency just for probes if not strictly necessary)
fn extract_json_str_value(json: &str, key: &str) -> Option<String> {
    if let Some(idx) = json.find(key) {
        let after_key = &json[idx + key.len()..];
        // Now find the first quote
        if let Some(val_start_idx) = after_key.find(':') {
            let after_colon = &after_key[val_start_idx + 1..];
            if let Some(quote_start) = after_colon.find('"') {
                let val_content = &after_colon[quote_start + 1..];
                if let Some(quote_end) = val_content.find('"') {
                    return Some(val_content[..quote_end].to_string());
                }
            }
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_docker_version() {
        let response = b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"Platform\":{\"Name\":\"Docker Engine - Community\"},\"Components\":[{\"Name\":\"Engine\",\"Version\":\"20.10.12\",\"Details\":{\"ApiVersion\":\"1.41\",\"Arch\":\"amd64\",\"BuildTime\":\"2021-12-13T11:45:00.000000000Z\",\"Experimental\":\"false\",\"GitCommit\":\"e91ed57\",\"GoVersion\":\"go1.16.12\",\"KernelVersion\":\"5.15.0-53-generic\",\"MinAPIVersion\":\"1.12\",\"Os\":\"linux\"}}],\"Version\":\"20.10.12\",\"ApiVersion\":\"1.41\",\"MinAPIVersion\":\"1.12\",\"GitCommit\":\"e91ed57\",\"GoVersion\":\"go1.16.12\",\"Os\":\"linux\",\"Arch\":\"amd64\",\"KernelVersion\":\"5.15.0-53-generic\",\"BuildTime\":\"2021-12-13T11:45:00.000000000Z\"}";
        let info = parse_docker_version_response(response).unwrap();
        assert_eq!(info.service, "docker");
        assert_eq!(info.version.unwrap(), "Docker Engine 20.10.12 (linux/amd64)");
    }

    #[test]
    fn test_parse_invalid() {
        let response = b"HTTP/1.1 404 Not Found\r\n\r\n";
        assert!(parse_docker_version_response(response).is_none());
    }
}