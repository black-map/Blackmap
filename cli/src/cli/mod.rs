//! Command Line Interface (CLI) configuration and parsing.
//!
//! Provides structures to parse arguments using `clap`.

use clap::{Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

/// Command line arguments for BlackMap
#[derive(Parser, Debug)]
#[command(name = "BlackMap")]
#[command(author, version = "5.1.2")]
#[command(about = "Fast, stealthy network reconnaissance framework with native fingerprint detection", long_about = None)]
pub struct Args {
    #[command(subcommand)]
    pub command: Commands,

    /// Verbosity level (0-3)
    #[arg(short = 'v', long, action = clap::ArgAction::Count, global = true)]
    pub verbose: u8,
}

#[derive(Subcommand, Debug)]
pub enum Commands {
    /// Port scanning and host discovery
    Scan {
        /// Target(s) to scan (IP, hostname, CIDR range, domain)
        #[arg(value_name = "TARGET")]
        target: Vec<String>,

        /// Ports to scan (e.g., 22,80,443 or 1-1000 or - for all)
        #[arg(short = 'p', long, value_name = "PORTS", default_value = "1-1000")]
        ports: String,

        /// Service version detection
        #[arg(short = 'V', long)]
        service_version: bool,

        /// OS detection
        #[arg(short = 'O', long)]
        os_detection: bool,

        /// Scan type
        #[arg(short = 's', long, value_enum, default_value = "tcp-connect")]
        scan_type: ScanTypeArg,

        /// Stealth level (0-5)
        #[arg(long, value_name = "LEVEL", default_value = "1")]
        stealth: u32,

        /// Number of concurrent connections
        #[arg(long, short = 't', value_name = "NUM", default_value = "500")]
        threads: u32,

        /// Connection timeout in seconds
        #[arg(long, value_name = "SECS", default_value = "5")]
        timeout: u64,

        /// Rate limit (packets per second)
        #[arg(long, value_name = "PPS")]
        rate_limit: Option<u32>,

        /// Enable adaptive rate control (auto-adjust based on packet loss/latency)
        #[arg(long)]
        adaptive_rate: bool,

        /// Minimum packets per second (used with --adaptive-rate or manual rate)
        #[arg(long, value_name = "PPS")]
        min_rate: Option<u32>,

        /// Maximum packets per second (used with --adaptive-rate or manual rate)
        #[arg(long, value_name = "PPS")]
        max_rate: Option<u32>,

        /// Global scan duration in seconds (default 15)
        #[arg(long, value_name = "SECS", default_value = "15")]
        scan_duration: u64,

        /// Enable OS version estimation (requires -O)
        #[arg(long)]
        os_version: bool,

        /// Ultra-fast raw packet scan; skips service/os detection
        #[arg(long)]
        ultra: bool,

        /// Internet-scale pseudo-random scan (ZMap style)
        #[arg(long)]
        internet_scan: bool,

        /// Output file
        #[arg(short = 'o', long)]
        output: Option<PathBuf>,

        /// Output format
        #[arg(long, value_enum, default_value = "table")]
        format: OutputFormatArg,

        /// Skip host discovery
        #[arg(long)]
        skip_discovery: bool,

        /// DNS servers (comma-separated)
        #[arg(long)]
        dns: Option<String>,

        /// Enable JSON output
        #[arg(short = 'J', long)]
        json: bool,

        /// Enable XML output
        #[arg(short = 'X', long)]
        xml: bool,

        /// Timing template (paranoid, stealth, balanced, fast, aggressive, insane)
        #[arg(short = 'T', long, value_name = "TEMPLATE")]
        timing: Option<String>,

        /// Max retries per port
        #[arg(long, value_name = "NUM", default_value = "2")]
        max_retries: u32,

        /// Start as distributed master node binding to IP:PORT
        #[arg(long)]
        master: Option<String>,

        /// Start as distributed worker node connecting to MASTER_IP:PORT
        #[arg(long)]
        worker: Option<String>,

        /// Paranoid stealth mode (level 5)
        #[arg(long)]
        paranoid: bool,

        /// Evasion mode (adds jitter, alters timing)
        #[arg(long)]
        evasion: bool,

        /// Randomize target ports completely
        #[arg(long)]
        randomize: bool,

        /// Set decoy IPs to spoof (comma-separated: 192.168.0.1,192.168.0.2)
        #[arg(long, value_name = "IPs")]
        decoys: Option<String>,

        /// Set explicit source port
        #[arg(long, short = 'S', value_name = "PORT")]
        source_port: Option<u16>,
    },
    
    /// Subdomain enumeration via DNS brute-force
    Subdomains {
        /// Target domain (e.g., example.com)
        #[arg(value_name = "DOMAIN")]
        domain: String,
        
        /// Number of concurrent DNS threads
        #[arg(long, short = 't', value_name = "NUM", default_value = "50")]
        threads: usize,
    }
}



#[derive(ValueEnum, Clone, Debug)]
pub enum ScanTypeArg {
    /// TCP CONNECT scan
    #[value(name = "tcp-connect")]
    TcpConnect,

    /// TCP SYN scan (requires root)
    #[value(name = "tcp-syn")]
    TcpSyn,

    /// UDP scan
    #[value(name = "udp")]
    Udp,

    /// ICMP ping
    #[value(name = "icmp")]
    IcmpPing,
}

#[derive(ValueEnum, Clone, Debug)]
pub enum OutputFormatArg {
    /// Table (human-readable)
    Table,

    /// JSON format
    Json,

    /// XML format
    Xml,

    /// CSV format
    Csv,
}

impl From<ScanTypeArg> for blackmap::config::ScanType {
    fn from(arg: ScanTypeArg) -> Self {
        match arg {
            ScanTypeArg::TcpConnect => blackmap::config::ScanType::TcpConnect,
            ScanTypeArg::TcpSyn => blackmap::config::ScanType::TcpSyn,
            ScanTypeArg::Udp => blackmap::config::ScanType::Udp,
            ScanTypeArg::IcmpPing => blackmap::config::ScanType::IcmpPing,
        }
    }
}

impl From<OutputFormatArg> for String {
    fn from(arg: OutputFormatArg) -> Self {
        match arg {
            OutputFormatArg::Table => "table".to_string(),
            OutputFormatArg::Json => "json".to_string(),
            OutputFormatArg::Xml => "xml".to_string(),
            OutputFormatArg::Csv => "csv".to_string(),
        }
    }
}

/// Parse port specification into a vector of port numbers
pub fn parse_ports(spec: &str) -> blackmap::error::Result<Vec<u16>> {
    let mut ports = Vec::new();

    if spec == "-" {
        // All ports
        for p in 1..=65535 {
            ports.push(p);
        }
        return Ok(ports);
    }

    for part in spec.split(',') {
        let part = part.trim();

        if part.contains('-') {
            // Range
            let range_parts: Vec<&str> = part.split('-').collect();
            if range_parts.len() != 2 {
                return Err(blackmap::error::BlackMapError::ConfigError(
                    format!("Invalid port range: {}", part),
                ));
            }

            let start: u16 = range_parts[0].parse()
                .map_err(|_| blackmap::error::BlackMapError::ConfigError(
                    format!("Invalid port: {}", range_parts[0]),
                ))?;

            let end: u16 = range_parts[1].parse()
                .map_err(|_| blackmap::error::BlackMapError::ConfigError(
                    format!("Invalid port: {}", range_parts[1]),
                ))?;

            if start > end {
                return Err(blackmap::error::BlackMapError::ConfigError(
                    "Port range start > end".to_string(),
                ));
            }

            for p in start..=end {
                ports.push(p);
            }
        } else {
            // Single port
            let port: u16 = part.parse()
                .map_err(|_| blackmap::error::BlackMapError::ConfigError(
                    format!("Invalid port: {}", part),
                ))?;
            ports.push(port);
        }
    }

    Ok(ports)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_single_port() {
        let ports = parse_ports("80").unwrap();
        assert_eq!(ports, vec![80]);
    }

    #[test]
    fn test_parse_multiple_ports() {
        let ports = parse_ports("22,80,443").unwrap();
        assert_eq!(ports, vec![22, 80, 443]);
    }

    #[test]
    fn test_parse_port_range() {
        let ports = parse_ports("1-5").unwrap();
        assert_eq!(ports, vec![1, 2, 3, 4, 5]);
    }

    #[test]
    fn test_parse_mixed_ports() {
        let ports = parse_ports("80,443,1000-1003").unwrap();
        assert_eq!(ports, vec![80, 443, 1000, 1001, 1002, 1003]);
    }
}
