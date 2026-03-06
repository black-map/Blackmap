//! BlackMap 4.0 CLI Application
//!
//! Fast, stealthy network reconnaissance framework
//! 
//! Usage examples:
//!   blackmap -p 22,80,443 scanme.nmap.org
//!   blackmap -sV -A target.com
//!   blackmap -p- --stealth 3 192.168.0.0/16
//!   blackmap --threads 1000 -oJ results.json 1.1.1.0/24

use clap::{Parser, ValueEnum};
use blackmap::{config::*, scanner::Scanner, dns::DnsResolver, output::{OutputFormat, format_output}};
use std::path::PathBuf;
use std::time::Duration;

#[derive(Parser, Debug)]
#[command(name = "BlackMap")]
#[command(version = "4.0.0")]
#[command(about = "Fast, stealthy network reconnaissance framework", long_about = None)]
struct Args {
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

    /// Output file
    #[arg(short = 'o', long)]
    output: Option<PathBuf>,

    /// Output format
    #[arg(long, value_enum, default_value = "table")]
    format: OutputFormatArg,

    /// Verbosity level (0-3)
    #[arg(short = 'v', long, action = clap::ArgAction::Count)]
    verbose: u8,

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

    /// Timing template (-T0 to -T5)
    #[arg(short = 'T', value_name = "TEMPLATE")]
    timing: Option<char>,

    /// Max retries per port
    #[arg(long, value_name = "NUM", default_value = "2")]
    max_retries: u32,

    /// Start as distributed master node binding to IP:PORT
    #[arg(long)]
    master: Option<String>,

    /// Start as distributed worker node connecting to MASTER_IP:PORT
    #[arg(long)]
    worker: Option<String>,
}

#[derive(ValueEnum, Clone, Debug)]
enum ScanTypeArg {
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
enum OutputFormatArg {
    /// Table (human-readable)
    Table,

    /// JSON format
    Json,

    /// XML format
    Xml,

    /// CSV format
    Csv,
}

impl From<ScanTypeArg> for ScanType {
    fn from(arg: ScanTypeArg) -> Self {
        match arg {
            ScanTypeArg::TcpConnect => ScanType::TcpConnect,
            ScanTypeArg::TcpSyn => ScanType::TcpSyn,
            ScanTypeArg::Udp => ScanType::Udp,
            ScanTypeArg::IcmpPing => ScanType::IcmpPing,
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

#[tokio::main]
async fn main() -> blackmap::error::Result<()> {
    let args = Args::parse();

    // Initialize logging
    let verbosity_level = match args.verbose {
        0 => "info",
        1 => "debug",
        _ => "trace",
    };
    tracing_subscriber::fmt()
        .with_env_filter(format!("blackmap={}", verbosity_level))
        .init();

    // Validate arguments (skipped if running as master/worker without targets)
    if args.target.is_empty() && args.master.is_none() && args.worker.is_none() {
        eprintln!("Error: No target specified");
        std::process::exit(1);
    }

    if args.stealth > 5 {
        eprintln!("Error: Stealth level must be 0-5");
        std::process::exit(1);
    }

    // Parse ports
    let ports = parse_ports(&args.ports)?;
    if ports.is_empty() {
        eprintln!("Error: No ports to scan");
        std::process::exit(1);
    }

    // Build configuration
    let mut config = ScanConfig::new();
    config.targets = args.target;
    config.ports = ports;
    config.timeout = Duration::from_secs(args.timeout);
    config.concurrency = args.threads;
    config.stealth_level = args.stealth;
    config.service_detection = args.service_version;
    config.os_detection = args.os_detection;
    config.verbosity = args.verbose as u32;
    config.output_file = args.output;
    config.output_format = String::from(args.format);
    config.skip_discovery = args.skip_discovery;
    config.max_retries = args.max_retries;
    config.scan_type = ScanType::from(args.scan_type);

    if let Some(rate_limit) = args.rate_limit {
        config.rate_limit = rate_limit;
    }

    if let Some(dns_servers) = args.dns {
        config.dns_servers = dns_servers.split(',').map(|s| s.to_string()).collect();
    }

    // Apply timing template
    if let Some(timing_char) = args.timing {
        match timing_char {
            '0' => config.stealth_level = 5,  // Paranoid
            '1' => config.stealth_level = 4,  // Sneaky
            '2' => config.stealth_level = 3,  // Polite
            '3' => config.stealth_level = 1,  // Normal
            '4' => {
                config.concurrency = 1000;    // Aggressive
                config.stealth_level = 0;
            }
            '5' => {
                config.concurrency = 5000;    // Insane
                config.stealth_level = 0;
            }
            _ => {
                eprintln!("Error: Invalid timing template (use -T0 to -T5)");
                std::process::exit(1);
            }
        }
    }

    println!("BlackMap 4.0.0 - Fast network reconnaissance framework");
    println!("https://github.com/Brian-Rojo/Blackmap\n");

    // Distributed Mode Check
    if let Some(bind_addr) = args.master {
        let master = blackmap::distributed::MasterNode::new(bind_addr, config);
        if let Err(e) = master.start().await {
            eprintln!("Master node error: {}", e);
        }
        return Ok(());
    }

    if let Some(master_addr) = args.worker {
        let worker = blackmap::distributed::WorkerNode::new(master_addr);
        if let Err(e) = worker.start().await {
            eprintln!("Worker node error: {}", e);
        }
        return Ok(());
    }

    // Initialize resolver
    let resolver = match DnsResolver::with_defaults().await {
        Ok(r) => r,
        Err(e) => {
            eprintln!("Error initializing DNS resolver: {}", e);
            std::process::exit(1);
        }
    };

    println!("[+] Resolving targets...");
    for target in &config.targets {
        println!("  Target: {}", target);
    }

    println!("[+] Configuration:");
    println!("  Ports: {} ports to scan", config.ports.len());
    println!("  Concurrency: {} threads", config.concurrency);
    println!(
"  Stealth: level {} (0=aggressive, 5=paranoid)",
        config.stealth_level
    );
    println!("  Service detection: {}", config.service_detection);
    println!("  OS detection: {}", config.os_detection);
    println!("  Timeout: {}s", args.timeout);

    // Create and run scanner
    let output_fmt_str = config.output_format.clone();
    let output_file = config.output_file.clone();
    let scanner = Scanner::new(config);

    println!("\n[+] Starting scan...\n");
    let start = std::time::Instant::now();

    match scanner.scan().await {
        Ok(result) => {
            let elapsed = start.elapsed();
            println!(
                "\n[+] Scan complete in {:.2}s",
                elapsed.as_secs_f64()
            );
            println!(
                "  Hosts found: {} up",
                result.stats.hosts_up
            );
            println!(
                "  Open ports: {}",
                result.stats.open_ports
            );

            // Format and output results
            let output_fmt_enum = match output_fmt_str.as_str() {
                "json" => OutputFormat::Json,
                "xml" => OutputFormat::Xml,
                "csv" => OutputFormat::Csv,
                _ => OutputFormat::Table,
            };

            match format_output(&result, output_fmt_enum, output_file.as_ref()).await {
                Ok(formatted) => {
                    if output_file.is_none() {
                        println!("\n{}", formatted);
                    } else {
                        println!("\n[+] Results written to {:?}", output_file);
                    }
                }
                Err(e) => {
                    eprintln!("[-] Error formatting output: {}", e);
                }
            }
        }
        Err(e) => {
            eprintln!("\n[-] Scan failed: {}", e);
            std::process::exit(1);
        }
    }

    Ok(())
}

/// Parse port specification
fn parse_ports(spec: &str) -> blackmap::error::Result<Vec<u16>> {
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
