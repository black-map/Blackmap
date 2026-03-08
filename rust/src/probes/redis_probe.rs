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
        // Send INFO command directly to get maximum information immediately
        let info_cmd = b"INFO\r\n";
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(info_cmd)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 4096];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                parse_redis_info(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses the Redis INFO response (usually a Bulk String containing key:value properties)
pub fn parse_redis_info(buffer: &[u8]) -> Option<ServiceInfo> {
    let response = String::from_utf8_lossy(buffer);
    
    // Redis Bulk Strings start with $<length>\r\n or -ERR or +OK
    // An INFO response starts with $<length> then content
    if !response.starts_with('$') && !response.starts_with("# Server") && !response.contains("redis_version:") {
        // If it responds with -ERR operation not permitted, we still found redis
        if response.starts_with("-ERR") || response.starts_with("-NOAUTH") {
            return Some(ServiceInfo {
                service: "redis".to_string(),
                version: None,
                confidence: 85,
            });
        }
        return None;
    }

    let mut redis_version = None;
    let mut os = None;

    for line in response.lines() {
        let line = line.trim();
        if line.starts_with("redis_version:") {
            redis_version = Some(line.trim_start_matches("redis_version:").to_string());
        } else if line.starts_with("os:") {
            os = Some(line.trim_start_matches("os:").to_string());
        }
    }

    if let Some(version) = redis_version {
        let mut full_ver = format!("Redis {}", version);
        if let Some(target_os) = os {
            full_ver.push_str(&format!(" ({})", target_os));
        }

        return Some(ServiceInfo {
            service: "redis".to_string(),
            version: Some(full_ver),
            confidence: 100,
        });
    }

    Some(ServiceInfo {
        service: "redis".to_string(),
        version: None,
        confidence: 90,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_redis_info() {
        let response = b"$3303\r\n# Server\r\nredis_version:7.0.5\r\nredis_git_sha1:00000000\r\nredis_git_dirty:0\r\nredis_build_id:a81234\r\nredis_mode:standalone\r\nos:Linux 5.15.0-53-generic x86_64\r\narch_bits:64\r\n";
        let info = parse_redis_info(response).unwrap();
        assert_eq!(info.service, "redis");
        assert_eq!(info.version.unwrap(), "Redis 7.0.5 (Linux 5.15.0-53-generic x86_64)");
    }

    #[test]
    fn test_parse_redis_auth_required() {
        let response = b"-NOAUTH Authentication required.\r\n";
        let info = parse_redis_info(response).unwrap();
        assert_eq!(info.service, "redis");
        assert!(info.version.is_none());
    }

    #[test]
    fn test_parse_invalid() {
        let response = b"HTTP/1.1 200 OK\r\n";
        assert!(parse_redis_info(response).is_none());
    }
}