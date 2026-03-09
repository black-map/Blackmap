//! Real OS Fingerprinting Engine
//!
//! Analyzes TTL, TCP window size, and service signatures for OS detection

#[derive(Debug, Clone)]
pub struct OSGuess {
    pub os_name: String,
    pub confidence: f32,
    pub signals: Vec<String>,
}

pub struct OSFingerprinter;

impl OSFingerprinter {
    /// Analyze TTL value to guess OS
    pub fn ttl_analysis(ttl: u8) -> Option<(String, f32)> {
        match ttl {
            100..=128 => Some(("Windows".to_string(), 85.0)),
            50..=64 => Some(("Linux/Unix".to_string(), 85.0)),
            200..=255 => Some(("Network Appliance".to_string(), 75.0)),
            _ => None,
        }
    }

    /// Analyze TCP window size to guess OS
    pub fn tcp_window_analysis(window_size: u16) -> Option<(String, f32)> {
        match window_size {
            8000..=32768 => Some(("Windows".to_string(), 70.0)),
            50000..=65535 => Some(("Linux".to_string(), 70.0)),
            5000..=7999 => Some(("BSD/MacOS".to_string(), 60.0)),
            _ => None,
        }
    }

    /// Analyze service banners for OS hints
    pub fn service_analysis(service: &str, banner: &str) -> Option<(String, f32)> {
        let lower = banner.to_lowercase();
        
        if lower.contains("debian") {
            return Some(("Debian Linux".to_string(), 90.0));
        }
        if lower.contains("ubuntu") {
            return Some(("Ubuntu Linux".to_string(), 90.0));
        }
        if lower.contains("redhat") || lower.contains("centos") {
            return Some(("RedHat/CentOS".to_string(), 85.0));
        }
        if lower.contains("windows") {
            return Some(("Windows".to_string(), 95.0));
        }
        if lower.contains("macos") || lower.contains("darwin") {
            return Some(("MacOS".to_string(), 90.0));
        }
        if lower.contains("freebsd") {
            return Some(("FreeBSD".to_string(), 90.0));
        }
        if lower.contains("openbsd") {
            return Some(("OpenBSD".to_string(), 90.0));
        }
        
        None
    }

    /// Combined analysis using multiple signals
    pub fn analyze_combined(
        ttl: Option<u8>,
        window_size: Option<u16>,
        service_banner: Option<&str>,
    ) -> OSGuess {
        let mut signals = Vec::new();
        let mut score_map = std::collections::HashMap::new();

        // TTL analysis
        if let Some(t) = ttl {
            if let Some((os, conf)) = Self::ttl_analysis(t) {
                *score_map.entry(os.clone()).or_insert(0.0) += conf;
                signals.push(format!("TTL {} → {}", t, os));
            }
        }

        // Window size analysis
        if let Some(w) = window_size {
            if let Some((os, conf)) = Self::tcp_window_analysis(w) {
                *score_map.entry(os.clone()).or_insert(0.0) += conf;
                signals.push(format!("TCP Window {} → {}", w, os));
            }
        }

        // Service banner analysis
        if let Some(banner) = service_banner {
            if let Some((os, conf)) = Self::service_analysis("", banner) {
                *score_map.entry(os.clone()).or_insert(0.0) += conf;
                signals.push(format!("Banner: {} → {}", banner, os));
            }
        }

        // Find highest scoring OS
        let (os_name, confidence) = score_map
            .iter()
            .fold(("Unknown".to_string(), 0.0), |(name, conf), (os, sc)| {
                if sc > &conf {
                    (os.clone(), *sc)
                } else {
                    (name, conf)
                }
            });

        // Normalize confidence
        let normalized_confidence = (confidence / ((ttl.is_some() as u32 + window_size.is_some() as u32 + service_banner.is_some() as u32) as f32)).min(100.0);

        OSGuess {
            os_name,
            confidence: normalized_confidence,
            signals,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ttl_windows() {
        let result = OSFingerprinter::ttl_analysis(110);
        assert!(result.is_some());
        let (os, conf) = result.unwrap();
        assert_eq!(os, "Windows");
        assert!(conf > 80.0);
    }

    #[test]
    fn test_ttl_linux() {
        let result = OSFingerprinter::ttl_analysis(64);
        assert!(result.is_some());
        let (os, _conf) = result.unwrap();
        assert_eq!(os, "Linux/Unix");
    }

    #[test]
    fn test_combined_analysis() {
        let guess = OSFingerprinter::analyze_combined(
            Some(64),
            Some(65000),
            Some("Ubuntu Linux"),
        );
        assert_eq!(guess.os_name, "Linux/Unix");
        assert!(!guess.signals.is_empty());
    }
}
