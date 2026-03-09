//! BlackMap Ultimate 6.3 CLI Application
//!
//! Fast, stealthy network reconnaissance framework with REAL CVE detection
//! Advanced scanning with service detection, OS fingerprinting, web technology detection, WAF detection, vulnerability detection
//! 
//! Usage examples:
//!   blackmap 192.168.1.1                          # Direct scan
//!   blackmap scanme.example.com -sV               # Service detection
//!   blackmap 192.168.0.0/24 -A                    # Aggressive scan
//!   blackmap scan 192.168.1.1 -p 22,80,443        # Port specific scan
//!   blackmap web scanme.example.com               # Web technology detection
//!   blackmap dns example.com                      # DNS reconnaissance

use clap::Parser;
use std::sync::Arc;
use std::io::{Read, Write};
use cli::cli::*;
use blackmap::{config::*, scanner::Scanner, dns::DnsResolver, output::{OutputFormat, format_output}};
use blackmap::subdomain_enum::{enumerate_subdomains, SubdomainResult};
use std::time::Duration;

#[tokio::main]
async fn main() -> blackmap::error::Result<()> {
    // custom help text copied from legacy v1.2.0 output and updated
    const HELP_TEXT: &str = r#"BlackMap Ultimate 6.3.0 (https://github.com/Brian-Rojo/Blackmap)
High‑Performance Network Reconnaissance Scanner

Usage:
  blackmap [Scan Type(s)] [Options] {target specification}

TARGET SPECIFICATION:
  Can pass hostnames, IP addresses, CIDR networks or ranges.
  Examples:
    blackmap scan scanme.example.com
    blackmap scan 192.168.1.1
    blackmap scan 192.168.1.0/24
    blackmap scan 10.0.0.1-254
    blackmap scan targets.txt

  -iL <file>            Input list of targets from file
  -iR <num hosts>       Scan random hosts
  --exclude <hosts>     Exclude hosts from scan
  --excludefile <file>  Exclude hosts listed in file


HOST DISCOVERY:
  -sn                   Ping scan only (disable port scanning)
  -Pn                   Treat all hosts as online
  -PE                   ICMP echo discovery
  -PS[ports]            TCP SYN discovery
  -PA[ports]            TCP ACK discovery
  --traceroute          Trace network path to host
  -n                    Disable DNS resolution
  -R                    Always resolve DNS


SCAN TECHNIQUES:
  -sS                   TCP SYN scan (stealth)
  -sT                   TCP connect scan
  -sU                   UDP scan
  -sA                   TCP ACK scan (firewall detection)
  -sW                   TCP window scan
  -sF                   FIN scan
  -sX                   Xmas scan
  -sN                   Null scan


PORT SPECIFICATION AND SCAN ORDER:
  -p <ports>            Scan specific ports
                        Examples:
                          -p22
                          -p1-1000
                          -p22,80,443
                          -p1-65535
  -p-                   Scan all 65535 ports
  --top-ports <n>       Scan top N most common ports
  --exclude-ports <p>   Exclude specified ports
  -F                    Fast scan (top 100 ports)
  -r                    Scan ports sequentially


SERVICE AND VERSION DETECTION:
  -sV                   Enable service version detection
  --version-intensity <0-9>
                        Set version detection intensity
  --version-light       Light detection (faster)
  --version-all         Aggressive detection
  --version-trace       Show probes and responses


OS DETECTION:
  -O                    Enable OS detection
  --osscan-limit        Limit OS detection to likely hosts
  --osscan-guess        Aggressive OS guessing


TIMING AND PERFORMANCE:
  -T0                   Paranoid (very slow)
  -T1                   Sneaky
  -T2                   Polite
  -T3                   Normal (default)
  -T4                   Aggressive
  -T5                   Insane (very fast)

  --threads <num>       Set concurrent scan threads
  --timeout <time>      Connection timeout
  --retries <num>       Retries for failed probes
  --min-rate <num>      Minimum packets per second
  --max-rate <num>      Maximum packets per second


STEALTH AND EVASION:
  --stealth <level>     Set stealth level (0‑5)
  -f                    Fragment packets
  --mtu <size>          Custom MTU for fragmentation
  -D <decoys>           Use decoy IP addresses
  -S <IP>               Spoof source address
  --source-port <port>  Use custom source port
  --ttl <value>         Set packet TTL
  --spoof-mac <mac>     Spoof MAC address


OUTPUT OPTIONS:
  -oN <file>            Output in normal format
  -oX <file>            Output in XML format
  -oJ <file>            Output in JSON format
  -oA <basename>        Output in all major formats

  -v                    Increase verbosity
  -vv                   Very verbose output
  -d                    Debug mode
  --reason              Show reason for port state
  --open                Show only open ports
  --packet-trace        Display packets sent/received


RECONNAISSANCE:
  recon <target>        Automated reconnaissance pipeline

  Performs:
    - Port scanning
    - Service detection
    - OS detection
    - Technology fingerprinting
    - CVE lookup


MISC:
  -6                    Enable IPv6 scanning
  --datadir <dir>       Custom data directory
  --update-db           Update service fingerprint database
  -V                    Print version information
  -h                    Show this help menu


EXAMPLES:

  Basic scan
    blackmap scan example.com

  Scan top 1000 ports
    blackmap scan example.com --top-ports 1000

  Full port scan
    blackmap scan example.com -p-

  Service detection
    blackmap scan example.com -sV

  Aggressive scan
    blackmap scan example.com -A

  Network scan
    blackmap scan 192.168.1.0/24

  Recon mode
    blackmap recon example.com


For more information and documentation:
https://github.com/Brian-Rojo/Blackmap
"#;

    let raw_args: Vec<String> = std::env::args().collect();
    // if no arguments supplied, show global help
    // also intercept -h/--help if not asking for a specific subcommand
    let wants_help = raw_args.iter().any(|a| a == "-h" || a == "--help");
    let subcommand = raw_args.get(1).map(String::as_str);
    let handle_global_help = raw_args.len() == 1 || (wants_help && match subcommand {
        Some("scan") | Some("subdomains") => false,
        _ => true,
    });

    if handle_global_help {
        println!("{}", HELP_TEXT);
        return Ok(());
    }

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

    match args.command {
        Commands::Scan {
            target,
            ports,
            service_version,
            os_detection,
            scan_type,
            stealth,
            threads,
            timeout,
            rate_limit,
            adaptive_rate,
            min_rate,
            max_rate,
            scan_duration,
            os_version,
            ultra,
            internet_scan,
            output,
            format,
            skip_discovery,
            dns,
            json,
            xml,
            timing,
            max_retries,
            master,
            worker,
            paranoid,
            evasion,
            randomize,
            decoys,
            source_port,
        } => {
            // Validate arguments
            if target.is_empty() && master.is_none() && worker.is_none() {
                eprintln!("Error: No target specified");
                std::process::exit(1);
            }

            if stealth > 5 && !paranoid {
                eprintln!("Error: Stealth level must be 0-5");
                std::process::exit(1);
            }

            let ports_parsed = parse_ports(&ports)?;
            if ports_parsed.is_empty() {
                eprintln!("Error: No ports to scan");
                std::process::exit(1);
            }

            // Build configuration
            let mut config = ScanConfig::new();
            config.targets = target;
            config.ports = ports_parsed;
            config.timeout = Duration::from_secs(timeout);
            config.concurrency = threads;
            config.stealth_level = stealth;
            config.service_detection = service_version;
            config.os_detection = os_detection;
            config.os_version_detection = os_version;
            config.ultra_mode = ultra;
            config.internet_scan = internet_scan;
            config.verbosity = args.verbose as u32;
            config.output_file = output;
            config.output_format = String::from(format);
            config.skip_discovery = skip_discovery;
            config.max_retries = max_retries;
            config.scan_type = ScanType::from(scan_type);

            config.adaptive_rate = adaptive_rate;
            config.min_rate = min_rate;
            config.max_rate = max_rate;
            config.max_duration = Some(Duration::from_secs(scan_duration));

            if ultra {
                // ultra mode overrides defaults to maximize raw packet rate
                config.service_detection = false;
                config.os_detection = false;
                if config.rate_limit == 0 {
                    config.rate_limit = 1_000_000;
                }
            }

            if internet_scan {
                // internet scan simply toggles random target generation later;
                // features like service/os detection are disabled
                config.os_detection = false;
                config.service_detection = false;
            }

            if paranoid {
                config.stealth_level = 5;
            }

            if evasion {
                // Ensure stealth is at least level 2 for jitter and evasion logic
                if config.stealth_level < 2 {
                    config.stealth_level = 2;
                }
            }

            if randomize {
                config.randomize_ports = true;
            }

            if let Some(mut d) = decoys {
                // Remove spaces and split commas
                d.retain(|c| !c.is_whitespace());
                config.decoys = d.split(',').filter(|x| !x.is_empty()).map(String::from).collect();
            }

            if let Some(sp) = source_port {
                config.source_port = Some(sp);
            }

            if let Some(rate) = rate_limit {
                config.rate_limit = rate;
            }

            if let Some(dns_servers) = dns {
                config.dns_servers = dns_servers.split(',').map(|s| s.to_string()).collect();
            }

            // Apply timing template
            if let Some(timing_str) = timing {
                match timing_str.to_lowercase().as_str() {
                    "paranoid" | "0" => config.stealth_level = 5,
                    "stealth" | "1" => config.stealth_level = 4,
                    "polite" | "2" => config.stealth_level = 3,
                    "balanced" | "3" => config.stealth_level = 1,
                    "fast" | "4" => {
                        config.concurrency = 1000;
                        config.stealth_level = 0;
                    }
                    "aggressive" | "insane" | "5" => {
                        config.concurrency = 5000;
                        config.stealth_level = 0;
                    }
                    _ => {
                        eprintln!("Error: Invalid timing template. Use: paranoid, stealth, polite, balanced, fast, aggressive");
                        std::process::exit(1);
                    }
                }
            }

            println!("BlackMap Ultimate 6.3.0 - Fast network reconnaissance framework");
            println!("https://github.com/Brian-Rojo/Blackmap\n");

            // Distributed Mode Check
            if let Some(bind_addr) = master {
                let master_node = blackmap::distributed::MasterNode::new(bind_addr, config);
                if let Err(e) = master_node.start().await {
                    eprintln!("Master node error: {}", e);
                }
                return Ok(());
            }

            if let Some(master_addr) = worker {
                let worker_node = blackmap::distributed::WorkerNode::new(master_addr);
                if let Err(e) = worker_node.start().await {
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
            for t in &config.targets {
                println!("  Target: {}", t);
            }

            println!("[+] Configuration:");
            println!("  Ports: {} ports to scan", config.ports.len());
            println!("  Concurrency: {} threads", config.concurrency);
            println!("  Stealth: level {} (0=aggressive, 5=paranoid)", config.stealth_level);
            println!("  Service detection: {}", config.service_detection);
            println!("  OS detection: {}", config.os_detection);
            println!("  Timeout: {}s", timeout);

            // Create and run scanner
            let output_fmt_str = config.output_format.clone();
            let output_file = config.output_file.clone();
            let scanner = Scanner::new(config);

            println!("\n[+] Starting scan...\n");
            let start = std::time::Instant::now();

            match scanner.scan().await {
                Ok(result) => {
                    let elapsed = start.elapsed();
                    println!("\n[+] Scan complete in {:.2}s", elapsed.as_secs_f64());
                    println!("  Hosts found: {} up", result.stats.hosts_up);
                    println!("  Open ports: {}", result.stats.open_ports);

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
                        Err(e) => eprintln!("[-] Error formatting output: {}", e),
                    }
                }
                Err(e) => {
                    eprintln!("\n[-] Scan failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
        
        Commands::Subdomains { domain, threads } => {
            println!("BlackMap Ultimate 6.3.0 - Subdomain Enumeration");
            println!("https://github.com/Brian-Rojo/Blackmap\n");
            println!("[+] Target: {}", domain);
            println!("[+] Threads: {}", threads);
            
            let resolver = DnsResolver::with_defaults().await?;
            let resolver_arc = Arc::new(resolver);
            
            println!("\n[+] Starting brute-force enumeration...\n");
            let start = std::time::Instant::now();
            
            match enumerate_subdomains(&domain, resolver_arc, threads).await {
                Ok(results) => {
                    let elapsed = start.elapsed();
                    println!("\n[+] Enumeration complete in {:.2}s: found {} subdomains", elapsed.as_secs_f64(), results.len());
                    
                    for res in results {
                        let ips: Vec<String> = res.ips.iter().map(|ip| ip.to_string()).collect();
                        println!("  {:30} => {}", res.subdomain, ips.join(", "));
                    }
                }
                Err(e) => {
                    eprintln!("[-] Enumeration failed: {}", e);
                    std::process::exit(1);
                }
            }
        }

        Commands::Web {
            target,
            https,
            http_port: _http_port,
            https_port: _https_port,
            timeout: _web_timeout,
            format: _web_format,
            output: _web_output,
        } => {
            println!("BlackMap Ultimate 6.3.0 - Web Technology Detection");
            println!("https://github.com/Brian-Rojo/Blackmap\n");
            println!("[+] Target: {}", target);
            println!("[+] Scanning for web technologies...\n");

            // Check HTTP on port 80
            println!("[*] Checking HTTP on port 80...");
            if let Ok(mut stream) = std::net::TcpStream::connect(format!("{}:{}", target, 80)) {
                let timeout_duration = Duration::from_secs(5);
                let _ = stream.set_read_timeout(Some(timeout_duration));
                let _ = stream.set_write_timeout(Some(timeout_duration));
                
                let request = format!("GET / HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", target);
                if let Ok(_) = std::io::Write::write_all(&mut stream, request.as_bytes()) {
                    let mut response = String::new();
                    let _ = std::io::Read::read_to_string(&mut stream, &mut response);
                    
                    for line in response.lines().take(20) {
                        if line.starts_with("Server:") {
                            println!("  [+] Server: {}", line.strip_prefix("Server:").unwrap_or("").trim());
                        }
                        if line.starts_with("X-Powered-By:") {
                            println!("  [+] Framework: {}", line.strip_prefix("X-Powered-By:").unwrap_or("").trim());
                        }
                    }
                }
            } else {
                println!("  [*] Port 80 not accessible");
            }

            if https {
                println!("[*] Checking HTTPS on port 443...");
                if std::net::TcpStream::connect(format!("{}:{}", target, 443)).is_ok() {
                    println!("  [+] HTTPS is available");
                } else {
                    println!("  [*] Port 443 not accessible");
                }
            }

            println!("\n[+] Web technology detection complete");
        }

        Commands::Dns {
            domain,
            threads,
            records: _show_records,
            dns_server: _dns_server,
            format: _dns_format,
            output: _dns_output,
        } => {
            println!("BlackMap Ultimate 6.3.0 - DNS Reconnaissance");
            println!("https://github.com/Brian-Rojo/Blackmap\n");
            println!("[+] Target domain: {}", domain);
            println!("[+] Threads: {}", threads);
            println!("[+] Starting DNS brute-force enumeration...\n");
            
            let resolver = DnsResolver::with_defaults().await?;
            let resolver_arc = Arc::new(resolver);
            
            let start = std::time::Instant::now();
            match enumerate_subdomains(&domain, resolver_arc, threads).await {
                Ok(results) => {
                    let elapsed = start.elapsed();
                    println!("[+] Found {} subdomains in {:.2}s:\n", results.len(), elapsed.as_secs_f64());
                    
                    for res in results {
                        let ips: Vec<String> = res.ips.iter().map(|ip| ip.to_string()).collect();
                        println!("  {:40} => {}", res.subdomain, ips.join(", "));
                    }
                }
                Err(e) => {
                    eprintln!("[-] DNS enumeration failed: {}", e);
                    std::process::exit(1);
                }
            }
        }
    }

    Ok(())
}

