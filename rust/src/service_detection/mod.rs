//! Service detection and OS fingerprinting
//!
//! Detects services and operating systems based on:
//! - Banner grabbing
//! - Protocol analysis
//! - Response fingerprinting
//! - TTL analysis

use serde::{Deserialize, Serialize};
use regex::Regex;
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use once_cell::sync::Lazy;
use tracing::{warn, debug};

/// Service detection result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServiceInfo {
    /// Service name
    pub service: String,

    /// Product name
    pub product: Option<String>,

    /// Detected version
    pub version: Option<String>,

    /// Raw banner/response
    pub banner: String,

    /// Confidence score (0-100)
    pub confidence: u8,

    /// Extra metadata
    pub metadata: Option<HashMap<String, String>>,
}


#[derive(Debug, Clone, Deserialize)]
struct JsonFingerprint {
    service: String,
    ports: Vec<u16>,
    patterns: Vec<String>,
    version_regex: Option<String>,
    version_template: Option<String>,
    confidence: u8,
}

/// Service fingerprint pattern (compiled)
struct ServicePattern {
    service: String,
    ports: Vec<u16>,
    patterns: Vec<Regex>,
    version_regex: Option<Regex>,
    version_template: Option<String>,
    confidence: u8,
}

// Fingerprint database (lazy-loaded from JSON)
static SERVICE_PATTERNS: Lazy<Vec<ServicePattern>> = Lazy::new(|| {
    let mut compiled_patterns = Vec::new();
    
    // Attempt to load from JSON DB
    let path = Path::new("data/fingerprints.json");
    if let Ok(content) = fs::read_to_string(path) {
        if let Ok(json_db) = serde_json::from_str::<Vec<JsonFingerprint>>(&content) {
            for entry in json_db {
                let mut valid_patterns = Vec::new();
                for p_str in entry.patterns {
                    match Regex::new(&p_str) {
                        Ok(re) => valid_patterns.push(re),
                        Err(_) => {
                            // Suppress warnings for unsupported PCRE syntax in nmap signatures
                        }
                    }
                }
                
                if !valid_patterns.is_empty() {
                    let v_regex = entry.version_regex.and_then(|r| Regex::new(&r).ok());
                    
                    compiled_patterns.push(ServicePattern {
                        service: entry.service,
                        ports: entry.ports,
                        patterns: valid_patterns,
                        version_regex: v_regex,
                        version_template: entry.version_template,
                        confidence: entry.confidence,
                    });
                }
            }
            debug!("Loaded {} service signatures from DB.", compiled_patterns.len());
        } else {
            warn!("Failed to parse data/fingerprints.json");
        }
    } else {
        warn!("data/fingerprints.json not found. Using fallback signatures.");
        
        // Fallback robust minimal set
        compiled_patterns.push(ServicePattern {
            service: "SSH".to_string(),
            ports: vec![22],
            patterns: vec![Regex::new(r"^SSH-\d+\.\d+-(.+)").unwrap()],
            version_regex: Some(Regex::new(r"OpenSSH[_\s]+([0-9.]+)").unwrap()),
            version_template: None,
            confidence: 99,
        });
        compiled_patterns.push(ServicePattern {
            service: "HTTP".to_string(),
            ports: vec![80, 8080, 8000],
            patterns: vec![Regex::new(r"HTTP/\d+\.\d+").unwrap()],
            version_regex: None,
            version_template: None,
            confidence: 95,
        });
    }

    compiled_patterns
});

/// Service detector
pub struct ServiceDetector;

impl ServiceDetector {
    /// Detect service from banner
    pub fn detect_from_banner(banner: &str) -> ServiceInfo {
        for pattern in SERVICE_PATTERNS.iter() {
            for regex in &pattern.patterns {
                if let Some(captures) = regex.captures(banner) {
                    let mut version = None;
                    
                    // Nmap specific: templates like p/vendor/ v/version/
                    if let Some(template) = &pattern.version_template {
                        if !template.is_empty() {
                            // Handle simplistic capture group mapping if it matches $1
                            if template.contains("$1") && captures.len() > 1 {
                                version = captures.get(1).map(|m| template.replace("$1", m.as_str()));
                            } else {
                                version = Some(template.clone());
                            }
                        }
                    } else if let Some(v_re) = &pattern.version_regex {
                        // Fallback fallback version parser
                        version = v_re.captures(banner)
                            .and_then(|caps| caps.get(1))
                            .map(|m| m.as_str().to_string());
                    }

                    return ServiceInfo {
                        service: pattern.service.clone(),
                        product: None,
                        version,
                        banner: banner.to_string(),
                        confidence: pattern.confidence,
                        metadata: None,
                    };
                }
            }
        }

        // Unknown service
        ServiceInfo {
            service: "Unknown".to_string(),
            product: None,
            version: None,
            banner: banner.to_string(),
            confidence: 0,
            metadata: None,
        }
    }
}


#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ssh_detection() {
        let banner = "SSH-2.0-OpenSSH_7.4";
        let info = ServiceDetector::detect_from_banner(banner);
        assert_eq!(info.service, "SSH");
    }
}
