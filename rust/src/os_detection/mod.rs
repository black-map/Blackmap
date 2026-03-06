//! Operating system fingerprinting
//!
//! Detects operating systems based on heuristics like TTL and TCP window size.

use serde::{Deserialize, Serialize};

/// Operating system detection result
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub enum OsType {
    /// Linux kernel
    Linux,

    /// Windows OS
    Windows,

    /// FreeBSD/OpenBSD/NetBSD
    Bsd,

    /// macOS
    MacOS,

    /// Network device (router, switch)
    NetworkDevice,

    /// Embedded system
    EmbeddedSystem,

    /// Unknown OS
    Unknown,
}

/// OS detection result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OsInfo {
    /// Detected OS type
    pub os_type: OsType,

    /// OS description
    pub description: String,

    /// Confidence score (0-100)
    pub confidence: u8,

    /// Detected TTL
    pub ttl: Option<u8>,
}

/// OS detector
pub struct OsDetector;

impl OsDetector {
    /// Detect OS from TTL value and window size
    pub fn detect_from_heuristics(ttl: u8, window_size: Option<u16>) -> OsInfo {
        let (os_type, confidence) = match ttl {
            64 => (OsType::Linux, 85),
            63 => (OsType::Linux, 80),
            128 => {
                // Windows typically has 128 TTL and specific window sizes like 8192 or 64240
                if let Some(ws) = window_size {
                    if ws == 8192 || ws == 64240 {
                        return OsInfo {
                            os_type: OsType::Windows,
                            description: format!("Detected via TTL {} and Window {}", ttl, ws),
                            confidence: 95,
                            ttl: Some(ttl),
                        };
                    }
                }
                (OsType::Windows, 90)
            },
            127 => (OsType::Windows, 85),
            254 | 255 => (OsType::NetworkDevice, 85),
            _ => (OsType::Unknown, 0),
        };

        OsInfo {
            os_type,
            description: format!("Detected via base TTL heuristics (TTL {})", ttl),
            confidence,
            ttl: Some(ttl),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_os_from_ttl() {
        let linux_info = OsDetector::detect_from_heuristics(64, Some(5840));
        assert_eq!(linux_info.os_type, OsType::Linux);

        let windows_info = OsDetector::detect_from_heuristics(128, Some(8192));
        assert_eq!(windows_info.os_type, OsType::Windows);
        assert_eq!(windows_info.confidence, 95);
    }
}
