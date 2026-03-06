//! Error handling for BlackMap

use std::io;
use std::fmt;

/// BlackMap result type
pub type Result<T> = std::result::Result<T, BlackMapError>;

/// BlackMap error types
#[derive(Debug)]
pub enum BlackMapError {
    /// DNS resolution failed
    DnsResolutionError(String),

    /// Network error
    NetworkError(String),

    /// Network I/O error
    IoError(io::Error),

    /// Invalid configuration
    ConfigError(String),

    /// Scanning error
    ScanError(String),

    /// Plugin loading error
    PluginError(String),

    /// Invalid target
    TargetError(String),

    /// Timeout error
    TimeoutError(String),

    /// Generic error
    Other(String),
}

impl fmt::Display for BlackMapError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            BlackMapError::DnsResolutionError(msg) => write!(f, "DNS resolution failed: {}", msg),
            BlackMapError::NetworkError(msg) => write!(f, "Network error: {}", msg),
            BlackMapError::IoError(err) => write!(f, "I/O error: {}", err),
            BlackMapError::ConfigError(msg) => write!(f, "Configuration error: {}", msg),
            BlackMapError::ScanError(msg) => write!(f, "Scan error: {}", msg),
            BlackMapError::PluginError(msg) => write!(f, "Plugin error: {}", msg),
            BlackMapError::TargetError(msg) => write!(f, "Target error: {}", msg),
            BlackMapError::TimeoutError(msg) => write!(f, "Timeout: {}", msg),
            BlackMapError::Other(msg) => write!(f, "Error: {}", msg),
        }
    }
}

impl std::error::Error for BlackMapError {}

impl From<io::Error> for BlackMapError {
    fn from(err: io::Error) -> Self {
        BlackMapError::IoError(err)
    }
}

impl From<serde_json::error::Error> for BlackMapError {
    fn from(err: serde_json::error::Error) -> Self {
        BlackMapError::Other(err.to_string())
    }
}
