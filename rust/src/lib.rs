//! BlackMap 4.0 - Fast, Stealthy Network Reconnaissance Framework
//! 
//! A professional-grade network scanning tool written in Rust with minimal C FFI.
//! Supports concurrent scanning, service detection, OS fingerprinting, and stealth techniques.

#![warn(missing_docs)]

// Core modules
pub mod config;
pub mod dns;
pub mod scanner;
pub mod probes;
pub mod cdn_detection;
pub mod waf_detection;
pub mod subdomain_enum;
pub mod output;

// Additional modules
pub mod scheduler;
pub mod plugin;
pub mod error;
pub mod ffi;
pub mod distributed;

// Re-exports for convenience
pub use config::ScanConfig;
pub use scanner::{Scanner, ScanResult};
pub use dns::DnsResolver;
pub use output::{OutputFormat, format_output};
pub use error::Result;

/// BlackMap version string
pub const VERSION: &str = "6.0.0";

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
