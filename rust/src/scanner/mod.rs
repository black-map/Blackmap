//! High-performance async scanning engine
//!
//! This module handles all scanning operations:
//! - TCP CONNECT scans
//! - TCP SYN scans (via C FFI)
//! - UDP scans
//! - Parallel port scanning
//! - Host discovery

use crate::config::{ScanConfig, ScanType};
use crate::dns::DnsResolver;
use crate::error::Result;
use serde::{Deserialize, Serialize};
use std::net::{IpAddr, SocketAddr};
use std::time::Duration;
use tokio::net::TcpStream;
use tokio::time::timeout;
use tokio::task::JoinSet;
use crate::banner_grabbing::grab_banner;
use crate::service_detection::ServiceDetector;
use crate::cdn_detection::{detect_cdn, CdnProvider};
use crate::waf_detection::{detect_waf, WafProvider};

/// Scan result for a single port
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PortScan {
    /// Target IP address
    pub ip: IpAddr,

    /// Port number
    pub port: u16,

    /// Port state
    pub state: PortState,

    /// Response time
    pub response_time: Option<Duration>,

    /// Service name (if detected)
    pub service: Option<String>,

    /// Service version (if detected)
    pub version: Option<String>,

    /// Confidence score for detection (0-100)
    pub confidence: Option<u8>,

    /// Detected CDN provider
    pub cdn: Option<String>,

    /// Detected WAF provider
    pub waf: Option<String>,
}

/// Overall scan result for all hosts
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScanResult {
    /// Scanned hosts with their results
    pub hosts: Vec<HostScan>,

    /// Scan statistics
    pub stats: ScanStats,

    /// Scan start time
    pub start_time: chrono::DateTime<chrono::Utc>,

    /// Scan end time
    pub end_time: chrono::DateTime<chrono::Utc>,
}

/// Individual host scan result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HostScan {
    /// Host IP or hostname
    pub host: String,

    /// Host is up
    pub is_up: bool,

    /// Open ports
    pub ports: Vec<PortScan>,

    /// Operating system (if detected)
    pub os: Option<String>,
}

/// Scan statistics
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ScanStats {
    /// Total hosts scanned
    pub total_hosts: usize,

    /// Hosts found up
    pub hosts_up: usize,

    /// Total ports scanned
    pub total_ports: usize,

    /// Open ports found
    pub open_ports: usize,

    /// Closed ports found
    pub closed_ports: usize,

    /// Filtered ports found
    pub filtered_ports: usize,
}

/// Port state
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum PortState {
    /// Port is open and accepting connections
    Open,

    /// Port is closed, connection refused
    Closed,

    /// Port is filtered, no response
    Filtered,

    /// Unknown state
    Unknown,
}

/// High-performance async scanner
pub struct Scanner {
    config: ScanConfig,
}

impl Scanner {
    /// Create a new scanner with configuration
    pub fn new(config: ScanConfig) -> Self {
        Self { config }
    }

    /// Start scanning
    pub async fn scan(&self) -> Result<ScanResult> {
        let start_time = chrono::Utc::now();
        let start = std::time::Instant::now();

        // Initialize DNS resolver
        let resolver = DnsResolver::with_defaults().await?;

        // Resolve all targets to IP addresses
        let mut all_ips = Vec::new();
        for target in &self.config.targets {
            match resolver.resolve(target).await {
                Ok(resolved) => {
                    tracing::info!(
                        "Resolved {} to {} address(es)",
                        target,
                        resolved.addresses.len()
                    );
                    all_ips.extend(resolved.addresses);
                }
                Err(e) => {
                    tracing::warn!("Failed to resolve {}: {}", target, e);
                }
            }
        }

        if all_ips.is_empty() {
            tracing::warn!("No valid targets after resolution");
            return Ok(ScanResult {
                hosts: Vec::new(),
                stats: ScanStats {
                    total_hosts: 0,
                    hosts_up: 0,
                    total_ports: 0,
                    open_ports: 0,
                    closed_ports: 0,
                    filtered_ports: 0,
                },
                start_time,
                end_time: chrono::Utc::now(),
            });
        }

        let total_hosts = all_ips.len();
        let total_ports = self.config.ports.len() * total_hosts;

        tracing::info!(
            "Starting scan: {} hosts, {} ports each, {} threads",
            total_hosts,
            self.config.ports.len(),
            self.config.concurrency
        );

        // Perform concurrent scans
        let mut host_scans = Vec::new();
        let mut total_open = 0;
        let mut total_closed = 0;
        let mut total_filtered = 0;
        let mut hosts_with_open_ports = 0;

        for ip in all_ips {
            let mut host_result = HostScan {
                host: ip.to_string(),
                is_up: false,
                ports: Vec::new(),
                os: None,
            };

            // Scan ports for this host
            let mut join_set: JoinSet<PortScan> = JoinSet::new();
            let timeout_duration = self.config.timeout;
            let concurrency = self.config.concurrency as usize;

            for (idx, &port) in self.config.ports.iter().enumerate() {
                // Manual concurrency limiting
                if idx % concurrency == 0 && idx > 0 {
                    // Let some tasks complete before spawning more
                    while join_set.len() > concurrency / 2 {
                        if let Some(result) = join_set.join_next().await {
                            if let Ok(port_result) = result {
                                match port_result.state {
                                    PortState::Open => total_open += 1,
                                    PortState::Closed => total_closed += 1,
                                    PortState::Filtered => total_filtered += 1,
                                    PortState::Unknown => {}
                                }
                                host_result.ports.push(port_result);
                            }
                        }
                    }
                }

                let addr = SocketAddr::new(ip, port);
                let max_retries = self.config.max_retries;
                let do_service_detection = self.config.service_detection;
                
                // Rate limiting logic
                let pps = self.config.rate_limit;
                if pps > 0 {
                    let delay_ns = 1_000_000_000 / pps as u64;
                    tokio::time::sleep(Duration::from_nanos(delay_ns)).await;
                }
                
                join_set.spawn(async move {
                    Self::scan_port_static(addr, timeout_duration, max_retries, do_service_detection).await
                });
            }

            // Collect remaining results
            while let Some(result) = join_set.join_next().await {
                if let Ok(port_result) = result {
                    match port_result.state {
                        PortState::Open => total_open += 1,
                        PortState::Closed => total_closed += 1,
                        PortState::Filtered => total_filtered += 1,
                        PortState::Unknown => {}
                    }
                    host_result.ports.push(port_result);
                }
            }

            // Check if host is up
            host_result.is_up = host_result.ports.iter().any(|p| p.state == PortState::Open);
            if host_result.is_up {
                hosts_with_open_ports += 1;
            }

            // Perform OS Detection if enabled
            if self.config.os_detection && host_result.is_up {
                let mut os_detected = None;
                
                // Try executing a ping to get TTL as a proxy for OS Fingerprinting since raw sockets aren't used here 
                if let Ok(output) = std::process::Command::new("ping")
                    .args(&["-c", "1", "-W", "2", &ip.to_string()])
                    .output()
                {
                    if output.status.success() {
                        let stdout = String::from_utf8_lossy(&output.stdout);
                        // Look for "ttl=" in the output
                        for word in stdout.split_whitespace() {
                            if word.starts_with("ttl=") || word.starts_with("TTL=") {
                                // Extract only digits to avoid parsing errors from attached characters (e.g. "ttl=250 ")
                                let ttl_str: String = word[4..].chars().filter(|c| c.is_digit(10)).collect();
                                if let Ok(ttl) = ttl_str.parse::<u8>() {
                                    let os_info = crate::os_detection::OsDetector::detect_from_heuristics(ttl, None);
                                    os_detected = Some(os_info.description);
                                    break;
                                }
                            }
                        }
                    }
                }
                
                if os_detected.is_none() {
                    os_detected = Some("Unknown (Ping blocked or requires root)".to_string());
                }
                
                host_result.os = os_detected;
            }

            host_scans.push(host_result);
        }

        let elapsed = start.elapsed();
        tracing::info!(
            "Scan complete in {:.2}s - {} open, {} closed, {} filtered",
            elapsed.as_secs_f64(),
            total_open,
            total_closed,
            total_filtered
        );

        let end_time = chrono::Utc::now();

        Ok(ScanResult {
            hosts: host_scans,
            stats: ScanStats {
                total_hosts,
                hosts_up: hosts_with_open_ports,
                total_ports,
                open_ports: total_open,
                closed_ports: total_closed,
                filtered_ports: total_filtered,
            },
            start_time,
            end_time,
        })
    }

    /// Scan a single port with timeout and retries (static version for spawn)
    async fn scan_port_static(addr: SocketAddr, timeout_dur: Duration, max_retries: u32, do_service_detection: bool) -> PortScan {
        let start = std::time::Instant::now();
        let mut attempts = 0;
        let mut final_state = PortState::Unknown;
        let mut response_time = None;

        while attempts <= max_retries {
            attempts += 1;
            match timeout(timeout_dur, TcpStream::connect(addr)).await {
                Ok(Ok(_stream)) => {
                    tracing::debug!("Port {}/{} open", addr.ip(), addr.port());
                    final_state = PortState::Open;
                    response_time = Some(start.elapsed());
                    break;
                }
                Ok(Err(e)) => {
                    // Connection refused = port closed immediately, no need to retry
                    if e.kind() == std::io::ErrorKind::ConnectionRefused {
                        final_state = PortState::Closed;
                        response_time = Some(start.elapsed());
                        break;
                    } else {
                        // Other errors, might retry
                        final_state = PortState::Filtered;
                    }
                }
                Err(_timeout) => {
                    // Timeout = filtered, will retry
                    final_state = PortState::Filtered;
                }
            };
            
            // Wait slightly before retry if we are going to retry
            if attempts <= max_retries && final_state == PortState::Filtered {
                tokio::time::sleep(Duration::from_millis(50)).await;
            }
        }

        if response_time.is_none() {
            response_time = Some(start.elapsed());
        }

        let mut service = None;
        let mut version = None;
        let mut confidence = None;
        let mut cdn_result = None;
        let mut waf_result = None;
        
        // If port is open and service detection is enabled, grab banner
        if final_state == PortState::Open {
             let default_service = match addr.port() {
                 20 | 21 => "ftp",
                 22 => "ssh",
                 23 => "telnet",
                 25 | 465 | 587 => "smtp",
                 53 => "domain",
                 80 | 8080 => "http",
                 110 | 995 => "pop3",
                 143 | 993 => "imap",
                 443 | 8443 => "https",
                 445 => "microsoft-ds",
                 3306 => "mysql",
                 3389 => "ms-wbt-server",
                 5432 => "postgresql",
                 8000 => "http-alt",
                 _ => "unknown",
             };
             
             service = Some(default_service.to_string());
             
             if do_service_detection {
                 if let Some(banner_res) = grab_banner(&addr.ip(), addr.port()).await {
                     if let Some(detected) = ServiceDetector::detect_from_banner(&banner_res.payload) {
                         service = Some(detected.service.clone());
                         version = detected.version;
                         confidence = Some(detected.confidence);
                     }
                     
                     // Deep Recon: CDN and WAF detection for HTTP/HTTPS
                     let is_http = addr.port() == 80 || addr.port() == 443 || 
                                   service.as_deref().unwrap_or("").to_uppercase() == "HTTP" ||
                                   service.as_deref().unwrap_or("").to_uppercase() == "HTTPS";
                                   
                     if is_http {
                         if let Some(cdn) = detect_cdn(&addr.ip().to_string(), &banner_res.payload) {
                             cdn_result = Some(format!("{:?}", cdn)); // simplified string format
                         }
                         if let Some(waf) = detect_waf(&banner_res.payload) {
                             waf_result = Some(format!("{:?}", waf));
                         }
                     }
                 }
             }
        }

        PortScan {
            ip: addr.ip(),
            port: addr.port(),
            state: final_state,
            response_time,
            service,
            version,
            confidence,
            cdn: cdn_result,
            waf: waf_result,
        }
    }

    /// Scan a single port with timeout
    async fn scan_port(&self, addr: SocketAddr, timeout_dur: Duration) -> Result<PortState> {
        let start = std::time::Instant::now();

        match timeout(timeout_dur, TcpStream::connect(addr)).await {
            Ok(Ok(_)) => {
                let elapsed = start.elapsed();
                tracing::debug!("Port {}/{} open ({}ms)", addr.ip(), addr.port(), elapsed.as_millis());
                Ok(PortState::Open)
            }
            Ok(Err(_)) => {
                Ok(PortState::Closed)
            }
            Err(_) => {
                Ok(PortState::Filtered)
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_port_state() {
        assert_eq!(PortState::Open, PortState::Open);
        assert_ne!(PortState::Open, PortState::Closed);
    }
}
