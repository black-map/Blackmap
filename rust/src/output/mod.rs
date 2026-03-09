//! Output formatting for scan results
//!
//! Supports multiple output formats:
//! - Table (human-readable)
//! - JSON
//! - XML
//! - CSV

use crate::scanner::ScanResult;
use serde_json::json;
use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

/// Output formatter
pub enum OutputFormat {
    /// Human-readable table
    Table,

    /// JSON format
    Json,

    /// XML format
    Xml,

    /// CSV format
    Csv,
}

/// Format and output scan results
pub async fn format_output(
    result: &ScanResult,
    format: OutputFormat,
    output_file: Option<&PathBuf>,
) -> crate::Result<String> {
    let formatted = match format {
        OutputFormat::Table => format_table(result),
        OutputFormat::Json => format_json(result)?,
        OutputFormat::Xml => format_xml(result),
        OutputFormat::Csv => format_csv(result),
    };

    // Write to file if specified
    if let Some(path) = output_file {
        let mut file = File::create(path)
            .map_err(|e| crate::error::BlackMapError::IoError(e))?;
        file.write_all(formatted.as_bytes())
            .map_err(|e| crate::error::BlackMapError::IoError(e))?;
    }

    Ok(formatted)
}

fn format_table(result: &ScanResult) -> String {
    let mut output = String::new();
    output.push_str("BlackMap Scan Report\n");
    output.push_str(&format!("Scan: {} to {}\n", result.start_time, result.end_time));
    output.push_str("\n");
    
    // Output details per host
    for host in &result.hosts {
        let has_open_ports = host.ports.iter().any(|p| p.state == crate::scanner::PortState::Open);
        
        if host.is_up || has_open_ports {
            output.push_str(&format!("Target: {} is UP\n", host.host));
            
            // INTEGRATION: Display OS Detection with confidence
            if let Some(os) = &host.os {
                if let Some(conf) = host.os_confidence {
                    output.push_str(&format!("OS Detected: {} ({}% confidence)\n", os, conf));
                } else {
                    output.push_str(&format!("OS Detected: {}\n", os));
                }
            }
            
            let has_open_ports = host.ports.iter().any(|p| p.state == crate::scanner::PortState::Open);
            
            if has_open_ports {
                output.push_str("PORT      STATE    SERVICE\n");
                
                for port in &host.ports {
                    if port.state == crate::scanner::PortState::Open {
                        let service = port.service.as_deref().unwrap_or("unknown");
                        let mut version_str = port.version.as_deref().unwrap_or("").to_string();
                        if let Some(conf) = port.confidence {
                            if !version_str.is_empty() {
                                version_str = format!("{} ({}% conf)", version_str, conf);
                            } else if conf > 0 {
                                version_str = format!("({}% conf)", conf);
                            }
                        }
                        
                        let mut extras = String::new();
                        if let Some(cdn) = &port.cdn {
                            extras.push_str(&format!("[CDN: {}] ", cdn));
                        }
                        if let Some(waf) = &port.waf {
                            extras.push_str(&format!("[WAF: {}] ", waf));
                        }
                        
                        // INTEGRATION: Display CVEs if detected
                        if let Some(cves) = &port.cves {
                            if !cves.is_empty() {
                                let cve_list = cves.join(", ");
                                extras.push_str(&format!("[CVEs: {}] ", cve_list));
                            }
                        }
                        
                        let port_str = format!("{}/tcp", port.port);
                        let mut service_detail = service.to_string();
                        if !version_str.is_empty() {
                            service_detail.push_str(&format!(" {}", version_str));
                        }
                        if !extras.is_empty() {
                            service_detail.push_str(&format!(" {}", extras.trim()));
                        }
                        
                        output.push_str(&format!(
                            "{:<9} {:<8} {}\n",
                            port_str,
                            "open",
                            service_detail
                        ));
                    }
                }
                output.push_str("\n");
            }
        }
    }

    output.push_str("--- Statistics ---\n");
    output.push_str(&format!("Hosts scanned: {}\n", result.stats.total_hosts));
    output.push_str(&format!("Hosts up: {}\n", result.stats.hosts_up));
    output.push_str(&format!("Open ports: {}\n", result.stats.open_ports));
    output
}

fn format_json(result: &ScanResult) -> crate::Result<String> {
    let json = json!({
        "scan_time": {
            "start": result.start_time,
            "end": result.end_time,
        },
        "statistics": {
            "total_hosts": result.stats.total_hosts,
            "hosts_up": result.stats.hosts_up,
            "total_ports": result.stats.total_ports,
            "open_ports": result.stats.open_ports,
            "closed_ports": result.stats.closed_ports,
            "filtered_ports": result.stats.filtered_ports,
        },
        "hosts": result.hosts,
    });

    Ok(serde_json::to_string_pretty(&json)?)
}

fn format_xml(result: &ScanResult) -> String {
    let mut output = String::from("<?xml version=\"1.0\"?>\n");
    output.push_str("<scan>\n");
    output.push_str(&format!("  <start>{}</start>\n", result.start_time));
    output.push_str(&format!("  <end>{}</end>\n", result.end_time));
    
    output.push_str("  <statistics>\n");
    output.push_str(&format!("    <total_hosts>{}</total_hosts>\n", result.stats.total_hosts));
    output.push_str(&format!("    <hosts_up>{}</hosts_up>\n", result.stats.hosts_up));
    output.push_str(&format!("    <open_ports>{}</open_ports>\n", result.stats.open_ports));
    output.push_str("  </statistics>\n");

    for host in &result.hosts {
        output.push_str("  <host>\n");
        output.push_str(&format!("    <address>{}</address>\n", host.host));
        output.push_str(&format!("    <status is_up=\"{}\" />\n", host.is_up));
        
        if let Some(os) = &host.os {
            output.push_str(&format!("    <os>{}</os>\n", os));
        }

        if !host.ports.is_empty() {
            output.push_str("    <ports>\n");
            for port in &host.ports {
                output.push_str(&format!("      <port num=\"{}\">\n", port.port));
                output.push_str(&format!("        <state>{:?}</state>\n", port.state));
                if let Some(service) = &port.service {
                    output.push_str(&format!("        <service name=\"{}\" ", service));
                    if let Some(version) = &port.version {
                        output.push_str(&format!("version=\"{}\" ", version));
                    }
                    if let Some(conf) = port.confidence {
                        output.push_str(&format!("confidence=\"{}\" ", conf));
                    }
                    output.push_str("/>\n");
                }
                output.push_str("      </port>\n");
            }
            output.push_str("    </ports>\n");
        }
        output.push_str("  </host>\n");
    }
    
    output.push_str("</scan>\n");
    output
}

fn format_csv(result: &ScanResult) -> String {
    let mut output = String::from("host,os,port,state,service,version,confidence\n");
    for host in &result.hosts {
        let os = host.os.as_deref().unwrap_or("unknown");
        for port in &host.ports {
            output.push_str(&format!(
                "{},\"{}\",{},{:?},\"{}\",\"{}\",{}\n",
                host.host,
                os,
                port.port,
                port.state,
                port.service.as_deref().unwrap_or("unknown"),
                port.version.as_deref().unwrap_or(""),
                port.confidence.unwrap_or(0)
            ));
        }
    }
    output
}

#[cfg(test)]
mod tests {
    use super::*;
    use chrono::Utc;

    fn create_test_result() -> ScanResult {
        ScanResult {
            hosts: vec![],
            stats: crate::scanner::ScanStats {
                total_hosts: 1,
                hosts_up: 1,
                total_ports: 1000,
                open_ports: 5,
                closed_ports: 995,
                filtered_ports: 0,
            },
            start_time: Utc::now(),
            end_time: Utc::now(),
        }
    }

    #[test]
    fn test_format_table() {
        let result = create_test_result();
        let output = format_table(&result);
        assert!(output.contains("Hosts scanned: 1"));
        assert!(output.contains("Open ports: 5"));
    }
}
