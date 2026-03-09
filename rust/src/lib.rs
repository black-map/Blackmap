//! BlackMap Ultimate 6.3 - Fast, Stealthy Network Reconnaissance Framework with Real CVE Detection
//! 
//! Enterprise-grade network scanning with vulnerability detection, advanced fingerprinting,
//! OS detection, web technology identification, and comprehensive reporting.

#![warn(missing_docs)]

// Core scanning modules
pub mod config;
pub mod dns;
pub mod scanner;
pub mod probes;
pub mod output;
pub mod error;
pub mod scheduler;
pub mod plugin;
pub mod ffi;
pub mod distributed;

// Service detection modules (v6.1+)
pub mod banner_grabber;
pub mod os_fingerprinter;
pub mod web_detector;
pub mod waf_detector;

// Advanced detection modules (v6.3+)
pub mod vulnerability_engine;
pub mod protocol_probes;
pub mod os_fingerprinter_new;
pub mod json_formatter;

// Legacy modules (compatibility)
pub mod cdn_detection;
pub mod waf_detection;
pub mod subdomain_enum;

// Re-exports for convenience
pub use config::ScanConfig;
pub use scanner::{Scanner, ScanResult};
pub use dns::DnsResolver;
pub use output::{OutputFormat, format_output};
pub use error::Result;
pub use banner_grabber::BannerGrabber;
pub use os_fingerprinter::OSDetector;
pub use web_detector::WebDetector;
pub use waf_detector::WAFDetector;
pub use vulnerability_engine::VulnerabilityEngine;
pub use protocol_probes::ProtocolProbes;
pub use os_fingerprinter_new::OSFingerprinter;
pub use json_formatter::ScanResult as JSONScanResult;

/// BlackMap version: 6.3.0 with real CVE detection
pub const VERSION: &str = "6.3.0";

/// Initialize BlackMap runtime
pub async fn init() -> Result<()> {
    // Initialize logging
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .init();

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_initialization() {
        assert_eq!(VERSION, "5.1.2");
    }
}
