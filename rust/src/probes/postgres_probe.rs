use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct PostgresProbe;

impl ServiceProbe for PostgresProbe {
    fn name(&self) -> &'static str {
        "postgresql"
    }

    fn ports(&self) -> Vec<u16> {
        vec![5432]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // PostgreSQL startup message (invalid user/database)
        // This will usually cause the server to respond with an ErrorResponse (E)
        // containing details about the server version in some cases, or at least confirmed PostgreSQL dialect.
        
        let startup_msg = [
            0x00, 0x00, 0x00, 0x21, // length (33)
            0x00, 0x03, 0x00, 0x00, // protocol version 3.0
            // parameters (key-value strings, null terminated)
            b'u', b's', b'e', b'r', 0,
            b'b', b'l', b'a', b'c', b'k', b'm', b'a', b'p', 0,
            0 // terminator
        ];
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(&startup_msg)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 1024];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                parse_postgres_response(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses the PostgreSQL response (typically an ErrorResponse to a bad startup message)
pub fn parse_postgres_response(buffer: &[u8]) -> Option<ServiceInfo> {
    if buffer.is_empty() {
        return None;
    }

    // Packet type: 'E' for ErrorResponse, 'R' for Authentication request
    let packet_type = buffer[0];
    
    // Some older or different Pgpool/PgBouncer formats might just send an Authentication reject immediately
    if packet_type == b'E' || packet_type == b'R' {
        let response_str = String::from_utf8_lossy(buffer);
        
        // Error messages often contain FATAL or PostgreSQL
        if response_str.contains("FATAL") || response_str.contains("PostgreSQL") || response_str.contains("pg_hba.conf") || response_str.contains("password authentication failed") || response_str.contains("role \"blackmap\" does not exist") {
            
            // Try to extract version from error details if any leakage point exists
            // Most modern Pg doesn't leak strict version in the auth failure, but we could detect pg bouncer or specific sub-dialects.
            let version = if response_str.contains("pgbouncer") {
                Some("PgBouncer".to_string())
            } else if response_str.contains("CockroachDB") {
                Some("CockroachDB".to_string())
            } else {
                Some("PostgreSQL".to_string()) // Generic but highly confident
            };

            return Some(ServiceInfo {
                service: "postgresql".to_string(),
                version,
                confidence: 95,
            });
        }
    }
    
    // SSL Negotiate response: 'N' or 'S' if we had sent an SSL request first (we just sent a StartupMessage here, but just in case)
    if buffer.len() == 1 && (buffer[0] == b'N' || buffer[0] == b'S') {
        return Some(ServiceInfo {
            service: "postgresql".to_string(),
            version: None,
            confidence: 80,
        });
    }

    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_postgres_error_response() {
        // Typically: 'E', length, 'S', 'FATAL', 0, 'C', '28000', 0, 'M', 'role "blackmap" does not exist', 0, ..., 0
        let response = b"E\x00\x00\x00\x55SFATAL\x00C28000\x00Mrole \"blackmap\" does not exist\x00Fauth.c\x00L285\x00Rauth_failed\x00\x00";
        let info = parse_postgres_response(response).unwrap();
        assert_eq!(info.service, "postgresql");
        assert_eq!(info.version.unwrap(), "PostgreSQL");
    }

    #[test]
    fn test_parse_cockroachdb() {
        let response = b"E\x00\x00\x00\x50SFATAL\x00Mpassword authentication failed for user \"blackmap\"\x00DCockroachDB\x00\x00";
        let info = parse_postgres_response(response).unwrap();
        assert_eq!(info.version.unwrap(), "CockroachDB");
    }

    #[test]
    fn test_parse_invalid() {
        let response = b"HTTP/1.1 400 Bad Request"; // Starts with H, not E or R
        assert!(parse_postgres_response(response).is_none());
    }
}