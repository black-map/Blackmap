//! Real JSON output formatter for scan results
//!
//! Serializes scan results, service detection, OS fingerprinting, and vulnerabilities to JSON

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PortResult {
    pub port: u16,
    pub protocol: String,
    pub service: Option<String>,
    pub version: Option<String>,
    pub state: String,
    pub os_guess: Option<String>,
    pub cves: Vec<String>,
    pub confidence: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScanResult {
    pub target: String,
    pub timestamp: String,
    pub duration_secs: f64,
    pub open_ports: usize,
    pub closed_ports: usize,
    pub filtered_ports: usize,
    pub ports: Vec<PortResult>,
    pub os_guess: Option<String>,
    pub os_confidence: Option<f32>,
    pub web_technologies: Vec<String>,
    pub waf_detected: Option<String>,
}

impl ScanResult {
    /// Create new scan result
    pub fn new(target: String) -> Self {
        ScanResult {
            target,
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs().to_string())
                .unwrap_or_else(|_| "unknown".to_string()),
            duration_secs: 0.0,
            open_ports: 0,
            closed_ports: 0,
            filtered_ports: 0,
            ports: Vec::new(),
            os_guess: None,
            os_confidence: None,
            web_technologies: Vec::new(),
            waf_detected: None,
        }
    }

    /// Add a port result
    pub fn add_port(&mut self, port: PortResult) {
        match port.state.as_str() {
            "open" => self.open_ports += 1,
            "closed" => self.closed_ports += 1,
            "filtered" => self.filtered_ports += 1,
            _ => {}
        }
        self.ports.push(port);
    }

    /// Convert to JSON string
    pub fn to_json(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string_pretty(self)
    }

    /// Convert to JSON with minimal formatting
    pub fn to_json_compact(&self) -> Result<String, serde_json::Error> {
        serde_json::to_string(self)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scan_result_creation() {
        let result = ScanResult::new("example.com".to_string());
        assert_eq!(result.target, "example.com");
        assert_eq!(result.open_ports, 0);
    }

    #[test]
    fn test_port_addition() {
        let mut result = ScanResult::new("example.com".to_string());
        result.add_port(PortResult {
            port: 80,
            protocol: "TCP".to_string(),
            service: Some("HTTP".to_string()),
            version: Some("Apache 2.4.38".to_string()),
            state: "open".to_string(),
            os_guess: Some("Linux".to_string()),
            cves: vec![],
            confidence: 95.0,
        });
        assert_eq!(result.open_ports, 1);
        assert_eq!(result.ports.len(), 1);
    }

    #[test]
    fn test_json_serialization() {
        let result = ScanResult::new("example.com".to_string());
        let json = result.to_json();
        assert!(json.is_ok());
    }
}
