//! OS Fingerprinting using TTL analysis, service signatures, and network behaviors
//! Supports detection of: Windows, Linux, macOS, BSD, Network devices, etc.

use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct OSFingerprint {
    pub os_name: String,
    pub os_family: String,
    pub confidence: u8, // 0-100%
    pub ttl: Option<u8>,
    pub window_size: Option<u16>,
    pub details: String,
}

pub struct OSDetector;

impl OSDetector {
    /// Detect OS based on TTL value
    pub fn detect_by_ttl(ttl: u8) -> (String, u8) {
        match ttl {
            // Linux/Unix typically use TTL 64
            50..=64 => ("Linux/Unix".to_string(), 75),
            // Windows typically uses TTL 128
            100..=128 => ("Windows".to_string(), 80),
            // Mac/iOS typically use TTL 64
            40..=40 => ("macOS/iOS".to_string(), 70),
            // Network devices and routers use 255
            200..=255 => ("Network Device/Router".to_string(), 65),
            // Default cases
            _ if ttl > 200 => ("Unknown Device".to_string(), 40),
            _ => ("Unknown OS".to_string(), 30),
        }
    }

    /// Detect OS based on TCP window size
    pub fn detect_by_window_size(window_size: u16) -> (String, u8) {
        match window_size {
            // Linux typically: 64240, 65160, 65535
            60000..=65535 => ("Linux/Unix".to_string(), 60),
            // Windows typically: 8192, 16384, 32768, 65535
            8000..=32768 => ("Windows".to_string(), 70),
            // macOS typically: 65535
            63000..=65535 => ("macOS/iOS".to_string(), 65),
            _ => ("Unknown".to_string(), 30),
        }
    }

    /// Detect OS based on service banners
    pub fn detect_by_service(service: &str, banner: &str) -> Option<(String, u8)> {
        let banner_lower = banner.to_lowercase();

        match service {
            "ssh" => {
                if banner_lower.contains("ubuntu") {
                    Some(("Linux (Ubuntu)".to_string(), 85))
                } else if banner_lower.contains("debian") {
                    Some(("Linux (Debian)".to_string(), 85))
                } else if banner_lower.contains("centos") || banner_lower.contains("rhel") {
                    Some(("Linux (CentOS/RHEL)".to_string(), 85))
                } else if banner_lower.contains("freebsd") {
                    Some(("FreeBSD".to_string(), 90))
                } else {
                    Some(("Linux/Unix".to_string(), 70))
                }
            }
            "smb" | "netbios" => {
                if banner_lower.contains("windows") {
                    Some(("Windows".to_string(), 95))
                } else {
                    Some(("Windows".to_string(), 80))
                }
            }
            "http" => {
                if banner_lower.contains("microsoft-iis") {
                    Some(("Windows (IIS)".to_string(), 90))
                } else if banner_lower.contains("apache") {
                    Some(("Linux/Unix (Apache)".to_string(), 80))
                } else if banner_lower.contains("nginx") {
                    Some(("Linux/Unix (Nginx)".to_string(), 75))
                } else if banner_lower.contains("server") {
                    Some(("Unknown".to_string(), 40))
                } else {
                    None
                }
            }
            _ => None,
        }
    }

    /// Combine multiple signals for better OS detection
    pub fn detect_combined(
        ttl: Option<u8>,
        window_size: Option<u16>,
        service: Option<&str>,
        banner: Option<&str>,
    ) -> OSFingerprint {
        let mut scores: HashMap<String, u16> = HashMap::new();
        let mut details = Vec::new();

        // TTL analysis
        if let Some(ttl_val) = ttl {
            let (os, confidence) = Self::detect_by_ttl(ttl_val);
            *scores.entry(os.clone()).or_insert(0) += confidence as u16;
            details.push(format!("TTL: {} ({}% confidence)", ttl_val, confidence));
        }

        // Window size analysis
        if let Some(ws) = window_size {
            let (os, confidence) = Self::detect_by_window_size(ws);
            *scores.entry(os.clone()).or_insert(0) += confidence as u16;
            details.push(format!("Window Size: {} ({}% confidence)", ws, confidence));
        }

        // Service/Banner analysis
        if let (Some(srv), Some(bann)) = (service, banner) {
            if let Some((os, confidence)) = Self::detect_by_service(srv, bann) {
                *scores.entry(os.clone()).or_insert(0) += confidence as u16 * 2; // Double weight
                details.push(format!("Service: {} ({}% confidence)", srv, confidence));
            }
        }

        // Find best match
        let (best_os, best_score) = scores
            .iter()
            .max_by_key(|entry| entry.1)
            .map(|(k, v)| (k.clone(), *v))
            .unwrap_or_else(|| ("Unknown OS".to_string(), 0));

        let final_confidence = (best_score / std::cmp::max(1, details.len() as u16)) as u8;

        let os_family = Self::get_os_family(&best_os);

        OSFingerprint {
            os_name: best_os,
            os_family,
            confidence: std::cmp::min(final_confidence, 99),
            ttl,
            window_size,
            details: details.join("\n"),
        }
    }

    /// Get OS family from OS name
    fn get_os_family(os_name: &str) -> String {
        if os_name.contains("Windows") {
            "Windows".to_string()
        } else if os_name.contains("Linux") || os_name.contains("Ubuntu") || os_name.contains("Debian") {
            "Linux".to_string()
        } else if os_name.contains("macOS") || os_name.contains("iOS") {
            "Apple".to_string()
        } else if os_name.contains("BSD") {
            "BSD".to_string()
        } else {
            "Unknown".to_string()
        }
    }

    /// Pretty print OS fingerprint
    pub fn format_os_detection(fp: &OSFingerprint) -> String {
        format!(
            "OS Guess: {} ({}% confidence)\nFamily: {}\n{}",
            fp.os_name, fp.confidence, fp.os_family, fp.details
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ttl_detection_windows() {
        let (os, conf) = OSDetector::detect_by_ttl(128);
        assert!(os.contains("Windows"));
        assert!(conf >= 70);
    }

    #[test]
    fn test_ttl_detection_linux() {
        let (os, conf) = OSDetector::detect_by_ttl(64);
        assert!(os.contains("Linux") || os.contains("Unix"));
        assert!(conf >= 70);
    }

    #[test]
    fn test_window_size_detection() {
        let (os, _conf) = OSDetector::detect_by_window_size(65535);
        assert!(!os.contains("Unknown"));
    }

    #[test]
    fn test_combined_detection() {
        let fp = OSDetector::detect_combined(
            Some(128),
            Some(32768),
            Some("http"),
            Some("Microsoft-IIS/10.0"),
        );
        assert!(fp.os_name.contains("Windows") || fp.confidence > 50);
    }
}
