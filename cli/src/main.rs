//! BlackMap 4.0 CLI Application
//!
//! Fast, stealthy network reconnaissance framework
//! 
//! Usage examples:
//!   blackmap -p 22,80,443 scanme.nmap.org
//!   blackmap -sV -A target.com
//!   blackmap -p- --stealth 3 192.168.0.0/16
//!   blackmap --threads 1000 -oJ results.json 1.1.1.0/24

use clap::Parser;
use std::sync::Arc;
use cli::cli::*;
use blackmap::{config::*, scanner::Scanner, dns::DnsResolver, output::{OutputFormat, format_output}};
use blackmap::subdomain_enum::{enumerate_subdomains, SubdomainResult};
use std::time::Duration;

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
            config.verbosity = args.verbose as u32;
            config.output_file = output;
            config.output_format = String::from(format);
            config.skip_discovery = skip_discovery;
            config.max_retries = max_retries;
            config.scan_type = ScanType::from(scan_type);

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

            println!("BlackMap 5.1.0 - Fast network reconnaissance framework");
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
            println!("BlackMap 5.1.0 - Subdomain Enumeration");
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
    }

    Ok(())
}

