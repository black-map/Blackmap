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
    output.push_str("</scan>\n");
    output
}

fn format_csv(result: &ScanResult) -> String {
    let mut output = String::from("host,port,state,service,version\n");
    for host in &result.hosts {
        for port in &host.ports {
            output.push_str(&format!(
                "{},{},{:?},\"{}\",%{}\n",
                host.host,
                port.port,
                port.state,
                port.service.as_deref().unwrap_or("unknown"),
                port.version.as_deref().unwrap_or("")
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
