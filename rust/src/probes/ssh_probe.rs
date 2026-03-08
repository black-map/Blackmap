use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct SshProbe;

impl ServiceProbe for SshProbe {
    fn name(&self) -> &'static str {
        "ssh"
    }

    fn ports(&self) -> Vec<u16> {
        vec![22, 2222]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // SSH banner grabbing
        let mut buffer = [0u8; 512];
        
        // Timeout handling for banner reads
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 0 => {
                parse_ssh_banner(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses an SSH banner (e.g., "SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.1")
pub fn parse_ssh_banner(buffer: &[u8]) -> Option<ServiceInfo> {
    let banner = String::from_utf8_lossy(buffer);
    let banner = banner.trim();

    // The banner may be preceded by other text, but the protocol line must start with SSH-
    // Let's find the first line containing "SSH-" (some servers send a pre-banner)
    let mut ssh_line = None;
    for line in banner.lines() {
        if line.starts_with("SSH-") {
            ssh_line = Some(line.trim());
            break;
        }
    }

    let ssh_line = ssh_line?;

    // Parse version, e.g., "SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.1"
    // Split by whitespace first for primary software vs comments
    let parts: Vec<&str> = ssh_line.split_whitespace().collect();
    if parts.is_empty() {
        return None;
    }

    let primary_part = parts[0]; // e.g., SSH-2.0-OpenSSH_8.9p1
    let extra_info = if parts.len() > 1 {
        Some(parts[1..].join(" "))
    } else {
        None
    };

    // Split by dashes, expect at least 3 parts: SSH, <protocol-version>, <software-version>
    let primary_segments: Vec<&str> = primary_part.split('-').collect();
    if primary_segments.len() >= 3 && primary_segments[0] == "SSH" {
        // The software version is everything from the 3rd segment onwards
        let software = primary_segments[2..].join("-");
        
        // Parse well-known SSH server software
        let mut version_info = if software.starts_with("OpenSSH_") {
            format!("OpenSSH {}", software.trim_start_matches("OpenSSH_"))
        } else if software.starts_with("dropbear_") {
            format!("Dropbear {}", software.trim_start_matches("dropbear_"))
        } else if software.starts_with("libssh_") {
            format!("libssh {}", software.trim_start_matches("libssh_"))
        } else if software.starts_with("RomSShell_") {
            format!("RomSShell {}", software.trim_start_matches("RomSShell_"))
        } else {
            software.clone()
        };

        // If there's extra info (usually OS info, e.g., Ubuntu, Debian, etc.)
        if let Some(extra) = extra_info {
            version_info.push_str(&format!(" ({})", extra));
        }

        return Some(ServiceInfo {
            service: "ssh".to_string(),
            version: Some(version_info),
            confidence: 95,
        });
    }

    // Fallback if it starts with SSH- but we can't parse it well
    Some(ServiceInfo {
        service: "ssh".to_string(),
        version: None,
        confidence: 85,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_openssh() {
        let banner = b"SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.1\r\n";
        let info = parse_ssh_banner(banner).unwrap();
        assert_eq!(info.service, "ssh");
        assert_eq!(info.version.unwrap(), "OpenSSH 8.9p1 (Ubuntu-3ubuntu0.1)");
    }

    #[test]
    fn test_parse_dropbear() {
        let banner = b"SSH-2.0-dropbear_2020.81\r\n";
        let info = parse_ssh_banner(banner).unwrap();
        assert_eq!(info.version.unwrap(), "Dropbear 2020.81");
    }

    #[test]
    fn test_parse_multiline_prebanner() {
        let banner = b"Warning: Unauthorized access prohibited\r\nSSH-2.0-OpenSSH_8.4\r\n";
        let info = parse_ssh_banner(banner).unwrap();
        assert_eq!(info.version.unwrap(), "OpenSSH 8.4");
    }

    #[test]
    fn test_parse_invalid() {
        let banner = b"HTTP/1.1 200 OK\r\n";
        assert!(parse_ssh_banner(banner).is_none());
    }
}