//! Configuration system for BlackMap

use serde::{Deserialize, Serialize};
use std::net::IpAddr;
use std::path::PathBuf;
use std::time::Duration;

/// Main scan configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScanConfig {
    /// Target hosts/IPs to scan
    pub targets: Vec<String>,

    /// Ports to scan
    pub ports: Vec<u16>,

    /// Scan timeout per connection
    pub timeout: Duration,

    /// Number of concurrent connections (threads)
    pub concurrency: u32,

    /// Stealth level (0-5)
    pub stealth_level: u32,

    /// Enable service detection
    pub service_detection: bool,

    /// Enable OS fingerprinting
    pub os_detection: bool,

    /// Enable verbose output
    pub verbosity: u32,

    /// Output file path
    pub output_file: Option<PathBuf>,

    /// Output format (json, xml, table, csv)
    pub output_format: String,

    /// DNS servers to use
    pub dns_servers: Vec<String>,

    /// Scan type (tcp-connect, tcp-syn, udp, etc.)
    pub scan_type: ScanType,

    /// Skip host discovery
    pub skip_discovery: bool,

    /// Rate limit (packets per second)
    pub rate_limit: u32,

    /// Probe timeout
    pub probe_timeout: Duration,

    /// Max retries per probe
    pub max_retries: u32,
}

/// Types of scans
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ScanType {
    /// TCP connect scan
    TcpConnect,

    /// TCP SYN scan (requires root)
    TcpSyn,

    /// TCP FIN scan
    TcpFin,

    /// TCP NULL scan
    TcpNull,

    /// TCP XMAS scan
    TcpXmas,

    /// UDP scan
    Udp,

    /// ICMP ping
    IcmpPing,

    /// TCP ACK ping
    TcpAckPing,

    /// Service detection only
    Service,
}

impl Default for ScanConfig {
    fn default() -> Self {
        Self {
            targets: Vec::new(),
            ports: vec![80, 443, 22, 3306, 5432], // Common ports
            timeout: Duration::from_secs(5),
            concurrency: 500,
            stealth_level: 1,
            service_detection: true,
            os_detection: false,
            verbosity: 0,
            output_file: None,
            output_format: "table".to_string(),
            dns_servers: vec!["8.8.8.8".to_string(), "8.8.4.4".to_string()],
            scan_type: ScanType::TcpConnect,
            skip_discovery: false,
            rate_limit: 0,
            probe_timeout: Duration::from_secs(5),
            max_retries: 2,
        }
    }
}

impl ScanConfig {
    /// Create a new configuration with defaults
    pub fn new() -> Self {
        Self::default()
    }

    /// Load configuration from a TOML file
    pub fn load_from_file(path: &PathBuf) -> crate::Result<Self> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| crate::error::BlackMapError::ConfigError(e.to_string()))?;

        toml::from_str(&content)
            .map_err(|e| crate::error::BlackMapError::ConfigError(e.to_string()))
    }

    /// Save configuration to a TOML file
    pub fn save_to_file(&self, path: &PathBuf) -> crate::Result<()> {
        let content = toml::to_string_pretty(self)
            .map_err(|e| crate::error::BlackMapError::ConfigError(e.to_string()))?;

        std::fs::write(path, content)
            .map_err(|e| crate::error::BlackMapError::ConfigError(e.to_string()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_default_config() {
        let cfg = ScanConfig::default();
        assert_eq!(cfg.concurrency, 500);
        assert_eq!(cfg.stealth_level, 1);
    }
}
