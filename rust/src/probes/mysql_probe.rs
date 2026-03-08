use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct MysqlProbe;

impl ServiceProbe for MysqlProbe {
    fn name(&self) -> &'static str {
        "mysql"
    }

    fn ports(&self) -> Vec<u16> {
        vec![3306]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // MySQL handshake packet
        let mut buffer = [0u8; 512];
        
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n >= 4 => {
                parse_mysql_handshake(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses the MySQL initial handshake packet
pub fn parse_mysql_handshake(buffer: &[u8]) -> Option<ServiceInfo> {
    if buffer.len() < 5 {
        return None;
    }

    // We do not enforce strict packet length equality here because TCP streams could be fragmented.
    // As long as we have enough bytes to read the version string, we proceed.

    // Sequence ID (1 byte)
    let sequence_id = buffer[3];
    if sequence_id != 0 {
        return None; // Initial handshake should have seq 0
    }

    // Protocol version (1 byte)
    let protocol_version = buffer[4];
    if protocol_version != 10 && protocol_version != 9 {
        return None; // Not a recognized MySQL protocol version
    }

    // Version string starts at offset 5 and is null-terminated
    let version_start = 5;
    let mut version_end = version_start;
    while version_end < buffer.len() && buffer[version_end] != 0 {
        version_end += 1;
    }

    if version_end > version_start && version_end < buffer.len() {
        let version_str = String::from_utf8_lossy(&buffer[version_start..version_end]);
        
        let version_final = if version_str.contains("MariaDB") {
            format!("MariaDB {}", version_str.replace("-MariaDB", ""))
        } else {
            format!("MySQL {}", version_str)
        };

        return Some(ServiceInfo {
            service: "mysql".to_string(),
            version: Some(version_final),
            confidence: 95,
        });
    }

    // If we couldn't parse the version string but it looks like MySQL
    Some(ServiceInfo {
        service: "mysql".to_string(),
        version: None,
        confidence: 85,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_mysql_handshake() {
        // Hex dump of a MySQL 8.0.31 handshake packet
        let packet = vec![
            0x4a, 0x00, 0x00, 0x00, // length = 74, seq = 0
            0x0a, // protocol 10
            // "8.0.31" null terminated
            0x38, 0x2e, 0x30, 0x2e, 0x33, 0x31, 0x00,
            // dummy data for rest of handshake...
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        ];
        
        let info = parse_mysql_handshake(&packet).unwrap();
        assert_eq!(info.service, "mysql");
        assert_eq!(info.version.unwrap(), "MySQL 8.0.31");
    }

    #[test]
    fn test_parse_mariadb_handshake() {
        let packet = vec![
            0x4a, 0x00, 0x00, 0x00, // length = 74, seq = 0
            0x0a, // protocol 10
            // "5.5.5-10.3.34-MariaDB" null terminated
            0x35, 0x2e, 0x35, 0x2e, 0x35, 0x2d, 0x31, 0x30, 0x2e, 0x33, 0x2e, 0x33, 0x34, 0x2d, 0x4d, 0x61, 0x72, 0x69, 0x61, 0x44, 0x42, 0x00
        ];
        
        let info = parse_mysql_handshake(&packet).unwrap();
        assert_eq!(info.service, "mysql");
        assert_eq!(info.version.unwrap(), "MariaDB 5.5.5-10.3.34");
    }

    #[test]
    fn test_parse_invalid() {
        let packet = vec![0x00, 0x00, 0x00];
        assert!(parse_mysql_handshake(&packet).is_none());
    }
}