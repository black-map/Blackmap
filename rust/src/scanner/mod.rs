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
use std::sync::Arc;
use tokio::sync::mpsc;

pub mod packet_parser;
pub mod port_state_tracker;
pub mod syn_receiver;
pub mod syn_sender;
pub mod target_scheduler;

use modules::banner_grabbing::grab_banner;
use modules::service_detection::ServiceDetector;
use crate::vulnerability_engine::VulnerabilityEngine;
use crate::os_fingerprinter_new::OSFingerprinter;
use rand::Rng;

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

    /// Detected CVEs for this service version (if any)
    pub cves: Option<Vec<String>>,

    /// CVE detection confidence (0-100)
    pub cve_confidence: Option<u8>,
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

    /// OS detection confidence (0-100)
    pub os_confidence: Option<u8>,
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
        let max_duration = self.config.max_duration.unwrap_or(Duration::from_secs(15));
        let deadline = std::time::Instant::now() + max_duration;

        // for elapsed reporting
        let start = std::time::Instant::now();

        // Initialize DNS resolver
        let resolver = DnsResolver::with_defaults().await?;

        // Resolve all targets to IP addresses
        let mut all_ips = Vec::new();
        if self.config.internet_scan {
            // Generate random IPs for internet scan
            use rand::Rng;
            let mut rng = rand::thread_rng();
            for _ in 0..1000 { // arbitrary number, can be configurable
                let ip = IpAddr::V4(std::net::Ipv4Addr::new(
                    rng.gen_range(1..255),
                    rng.gen_range(0..255),
                    rng.gen_range(0..255),
                    rng.gen_range(0..255),
                ));
                all_ips.push(ip);
            }
        } else {
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

        if self.config.scan_type == ScanType::TcpSyn {
            tracing::info!("Engaging Stateless Raw Socket TCP-SYN Engine (Requires Root)...");
            
            // 1. Initialize Scheduler & Tracker
            let scheduler = Arc::new(target_scheduler::TargetScheduler::new(all_ips.clone(), self.config.ports.clone()));
            let tracker = Arc::new(port_state_tracker::PortStateTracker::new());
            
            // 2. Initialize Sender & Receiver
            let sender = match syn_sender::SynSender::new(None) {
                Ok(s) => s,
                Err(e) => {
                    tracing::error!("Failed to initialize SYN Sender (Are you root?): {}", e);
                    std::process::exit(1);
                }
            };
            let receiver = match syn_receiver::SynReceiver::new(None) {
                Ok(r) => r,
                Err(e) => {
                    tracing::error!("Failed to initialize SYN Receiver (Are you root?): {}", e);
                    std::process::exit(1);
                }
            };
            
            let (shutdown_tx, shutdown_rx) = mpsc::channel(1);
            let tracker_clone = Arc::clone(&tracker);
            
            // 3. Spawn Receiver
            let receiver_task = tokio::task::spawn_blocking(move || {
                let rt = tokio::runtime::Builder::new_current_thread().enable_all().build().unwrap();
                rt.block_on(async {
                    if let Err(e) = receiver.run(tracker_clone, shutdown_rx).await {
                        tracing::error!("Receiver Error: {}", e);
                    }
                });
            });
            
            // 4. Run Sender
            let timeout_secs = self.config.timeout.as_secs_f32().ceil() as u64;
            if let Err(e) = sender.run(scheduler, Arc::clone(&tracker), self.config.rate_limit).await {
                tracing::error!("Sender Error: {}", e);
            }
            
            // 5. Wait for straggler SYN-ACKs (RTT delay)
            tokio::time::sleep(std::time::Duration::from_millis(timeout_secs * 1000 + 500)).await;
            
            // 6. Signal shutdown and await
            let _ = shutdown_tx.send(()).await;
            let _ = receiver_task.await;
            
            tracker.finalize_timeouts(timeout_secs * 1000);
            
            // 7. Process Results into standard format
            let results = tracker.get_results();
            
            for ip in all_ips.clone() {
                let my_results: Vec<&(IpAddr, u16, PortState)> = results.iter().filter(|(r_ip, _, _)| *r_ip == ip).collect();
                let my_open_ports: Vec<u16> = my_results.iter().filter(|(_, _, state)| *state == PortState::Open).map(|(_, p, _)| *p).collect();
                
                let mut host_alive = self.config.skip_discovery || !my_open_ports.is_empty();
                
                let mut host_result = HostScan {
                    host: ip.to_string(),
                    is_up: host_alive,
                    ports: Vec::new(),
                    os: None,
                    os_confidence: None,
                };
                
                for &p in &self.config.ports {
                    // Try to extract exact recorded state 
                    let recorded_state = my_results.iter()
                           .find(|(_, r_p, _)| *r_p == p)
                           .map(|(_, _, state)| *state)
                           .unwrap_or(PortState::Filtered); // Timeout / Missed = Filtered
                    
                    match recorded_state {
                        PortState::Open => {
                            total_open += 1;
                            
                            // For v5.1 architecture, service detections happen dynamically afterwards 
                            let mut service = None;
                            let mut version = None;
                            let mut confidence = None;
                            let mut cves = None;
                            let mut cve_confidence = None;
                            
                            if self.config.service_detection {
                                // Dynamic banner grabbing
                                if let Ok(Some(banner)) = timeout(Duration::from_secs(2), grab_banner(&ip, p)).await {
                                    if let Some(detected) = ServiceDetector::detect_from_banner(&banner.payload) {
                                        service = Some(detected.service.clone());
                                        version = detected.version.clone();
                                        confidence = Some(detected.confidence);
                                        
                                        // INTEGRATION: Check for CVEs based on service/version
                                        if let Ok(vuln_engine) = VulnerabilityEngine::load_from_file("data/cve_db.json") {
                                            if let Some(ver) = &detected.version {
                                                if let Some(vuln_match) = vuln_engine.check_vulnerabilities(&detected.service, ver) {
                                                    cves = Some(vuln_match.cves);
                                                    cve_confidence = Some(vuln_match.confidence as u8);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            
                            host_result.ports.push(PortScan {
                                ip,
                                port: p,
                                state: PortState::Open,
                                response_time: None,
                                service,
                                version,
                                confidence,
                                cdn: None,
                                waf: None,
                                cves,
                                cve_confidence,
                            });
                        },
                        PortState::Closed => total_closed += 1,
                        PortState::Filtered | PortState::Unknown => total_filtered += 1,
                    }
                }
                
                if host_alive {
                    hosts_with_open_ports += 1;
                }
                
                // INTEGRATION: OS Fingerprinting - analyze all port data for OS detection
                let (detected_os, os_conf) = Self::fingerprint_host_os(&host_result.ports);
                host_result.os = detected_os;
                host_result.os_confidence = os_conf;
                
                host_scans.push(host_result);
            }
        } else {
            let mut all_tasks = tokio::task::JoinSet::new();
            let semaphore = std::sync::Arc::new(tokio::sync::Semaphore::new(self.config.concurrency as usize));

            // Map IP to its initial HostScan config
            let mut host_map: std::collections::HashMap<IpAddr, HostScan> = std::collections::HashMap::new();

            for &ip in &all_ips {
                host_map.insert(ip, HostScan {
                    host: ip.to_string(),
                    is_up: false,
                    ports: Vec::new(),
                    os: None,
                    os_confidence: None,
                });
            }

            // Spawn all IP/Port combinations
            let timeout_duration = self.config.timeout;
            let max_retries = self.config.max_retries;
            let do_service_detection = self.config.service_detection;
            let rate_limit = self.config.rate_limit;
            
            for &ip in &all_ips {
                for &port in &self.config.ports {
                    if std::time::Instant::now() > deadline {
                        break;
                    }

                    // Block loop until we can spawn to avoid creating millions of tasks in memory at once
                    let permit = semaphore.clone().acquire_owned().await.unwrap();
                    let addr = SocketAddr::new(ip, port);
                     
                    all_tasks.spawn(async move {
                        if rate_limit > 0 {
                            let delay_ns = 1_000_000_000 / rate_limit as u64;
                            tokio::time::sleep(Duration::from_nanos(delay_ns)).await;
                        }
                        let res = Self::scan_port_static(addr, timeout_duration, max_retries, do_service_detection).await;
                        drop(permit);
                        res
                    });
                }
            }

            // Collect all results
            while let Some(result) = all_tasks.join_next().await {
                 if let Ok(port_result) = result {
                     match port_result.state {
                         PortState::Open => total_open += 1,
                         PortState::Closed => total_closed += 1,
                         PortState::Filtered => total_filtered += 1,
                         PortState::Unknown => {}
                     }
                     if let Some(host_entry) = host_map.get_mut(&port_result.ip) {
                         if port_result.state != PortState::Unknown {
                             host_entry.is_up = true;
                         }
                         host_entry.ports.push(port_result);
                     }
                 }
            }

            let mut all_tasks_os = tokio::task::JoinSet::new();
            
            // Perform OS Detection if enabled
            for (ip, mut host_result) in host_map {
                let mut needs_os = false;
                if self.config.skip_discovery {
                    host_result.is_up = true;
                }
                
                if host_result.is_up {
                    hosts_with_open_ports += 1;
                    if self.config.os_detection {
                        needs_os = true;
                    }
                }
                
                if needs_os {
                    // Spawn OS detection for hosts that are UP
                     all_tasks_os.spawn(async move {
                         // OS detection logic via ICMP TTL heuristic
                         let mut os_detected = None;
                         if let Ok(output) = std::process::Command::new("ping")
                             .args(&["-c", "1", "-W", "2", &ip.to_string()])
                             .output()
                         {
                             if output.status.success() {
                                 let stdout = String::from_utf8_lossy(&output.stdout);
                                 for word in stdout.split_whitespace() {
                                     if word.starts_with("ttl=") || word.starts_with("TTL=") {
                                         let ttl_str: String = word[4..].chars().filter(|c| c.is_digit(10)).collect();
                                         if let Ok(ttl) = ttl_str.parse::<u8>() {
                                             let os_info = modules::os_detection::OsDetector::detect_from_heuristics(ttl, None);
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
                         host_result
                     });
                } else {
                    host_scans.push(host_result);
                }
            }
            
            // Collect OS detection results
            while let Some(result) = all_tasks_os.join_next().await {
                if let Ok(host_result) = result {
                    host_scans.push(host_result);
                }
            }
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
        use tokio::net::TcpStream;
        let start = std::time::Instant::now();
        let mut attempts = 0;
        let mut final_state = PortState::Unknown;
        let mut response_time = None;
        let mut service = None;
        let mut version = None;
        let mut confidence = None;
        let mut cdn_result = None;
        let mut waf_result = None;
        let mut cves = None;
        let mut cve_confidence = None;

        while attempts <= max_retries {
            attempts += 1;
            match timeout(timeout_dur, TcpStream::connect(addr)).await {
                Ok(Ok(mut stream)) => {
                    tracing::debug!("Port {}/{} open", addr.ip(), addr.port());
                    final_state = PortState::Open;
                    response_time = Some(start.elapsed());

                    // Service detection using probes
                    if do_service_detection {
                        if let Some(service_info) = crate::probes::detect_service(addr.port(), &mut stream).await {
                            service = Some(service_info.service.clone());
                            version = service_info.version.clone();
                            confidence = Some(service_info.confidence as u8);
                            
                            // INTEGRATION: CVE Detection - match detected service/version against CVE database
                            if let Ok(vuln_engine) = VulnerabilityEngine::load_from_file("data/cve_db.json") {
                                if let Some(ver) = &service_info.version {
                                    if let Some(vuln_match) = vuln_engine.check_vulnerabilities(&service_info.service, ver) {
                                        cves = Some(vuln_match.cves);
                                        cve_confidence = Some(vuln_match.confidence as u8);
                                    }
                                }
                            }
                        }
                    }

                    // Fallback to default service
                    if service.is_none() {
                        service = Some(match addr.port() {
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
                        }.to_string());
                    }

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
            cves,
            cve_confidence,
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

    /// INTEGRATION: OS Fingerprinting - analyzes port scanning results to detect OS
    /// Uses multiple signals: service banners, port patterns
    fn fingerprint_host_os(ports: &[PortScan]) -> (Option<String>, Option<u8>) {
        // Collect service information from open ports
        let mut service_banners = Vec::new();
        
        for port in ports {
            if port.state == PortState::Open {
                if let Some(service) = &port.service {
                    if let Some(version) = &port.version {
                        service_banners.push(version.as_str());
                    }
                }
            }
        }
        
        // Try to detect OS from service signatures
        if !service_banners.is_empty() {
            for banner in &service_banners {
                if let Some((os, conf)) = OSFingerprinter::service_analysis("", banner) {
                    return (Some(os), Some(conf as u8));
                }
            }
        }
        
        // Fallback: Try to guess from common port patterns
        let has_ssh = ports.iter().any(|p| p.port == 22 && p.state == PortState::Open);
        let has_http = ports.iter().any(|p| (p.port == 80 || p.port == 8080) && p.state == PortState::Open);
        let has_rdp = ports.iter().any(|p| p.port == 3389 && p.state == PortState::Open);
        let has_smb = ports.iter().any(|p| p.port == 445 && p.state == PortState::Open);
        
        if has_rdp || has_smb {
            return (Some("Windows (Port pattern detection)".to_string()), Some(70));
        }
        
        if has_ssh && has_http {
            return (Some("Linux/Unix (Port pattern detection)".to_string()), Some(65));
        }
        
        (Some("Unknown (Insufficient fingerprint data)".to_string()), Some(0))
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
