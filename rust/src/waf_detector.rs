//! WAF (Web Application Firewall) Detection
//! Detects: Cloudflare, Akamai, AWS WAF, Imperva, Sucuri, ModSecurity, etc.

use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct WAFDetection {
    pub name: String,
    pub detected: bool,
    pub confidence: u8,
    pub indicators: Vec<String>,
}

pub struct WAFDetector;

impl WAFDetector {
    /// Detect WAF from HTTP headers and response patterns
    pub fn detect_waf(response: &str) -> Option<WAFDetection> {
        let response_lower = response.to_lowercase();
        let mut candidates: HashMap<String, (u8, Vec<String>)> = HashMap::new();

        // Cloudflare detection
        if response_lower.contains("cf-ray") || response_lower.contains("cloudflare") {
            candidates
                .entry("Cloudflare".to_string())
                .or_insert((0, vec![]))
                .0 += 40;
            candidates
                .entry("Cloudflare".to_string())
                .or_insert((0, vec![]))
                .1
                .push("CF-RAY header".to_string());
        }

        if response_lower.contains("cf-cache-status") {
            candidates
                .entry("Cloudflare".to_string())
                .or_insert((0, vec![]))
                .0 += 30;
            candidates
                .entry("Cloudflare".to_string())
                .or_insert((0, vec![]))
                .1
                .push("Cache status header".to_string());
        }

        // AWS WAF detection
        if response.contains("x-amzn-") || response_lower.contains("amaznwaf") {
            candidates
                .entry("AWS WAF".to_string())
                .or_insert((0, vec![]))
                .0 += 50;
            candidates
                .entry("AWS WAF".to_string())
                .or_insert((0, vec![]))
                .1
                .push("Amazon headers".to_string());
        }

        // Imperva/SecureSphere detection
        if response_lower.contains("imperva") || response_lower.contains("securesphere") {
            candidates
                .entry("Imperva".to_string())
                .or_insert((0, vec![]))
                .0 += 60;
            candidates
                .entry("Imperva".to_string())
                .or_insert((0, vec![]))
                .1
                .push("Imperva header".to_string());
        }

        if response_lower.contains("x-iinfo") {
            candidates
                .entry("Imperva".to_string())
                .or_insert((0, vec![]))
                .0 += 30;
            candidates
                .entry("Imperva".to_string())
                .or_insert((0, vec![]))
                .1
                .push("X-IINFO header".to_string());
        }

        // Akamai detection
        if response_lower.contains("akamai") || response_lower.contains("akamaized") {
            candidates
                .entry("Akamai".to_string())
                .or_insert((0, vec![]))
                .0 += 50;
            candidates
                .entry("Akamai".to_string())
                .or_insert((0, vec![]))
                .1
                .push("Akamai header".to_string());
        }

        // Sucuri detection
        if response_lower.contains("sucuri") {
            candidates
                .entry("Sucuri".to_string())
                .or_insert((0, vec![]))
                .0 += 55;
            candidates
                .entry("Sucuri".to_string())
                .or_insert((0, vec![]))
                .1
                .push("Sucuri header".to_string());
        }

        // ModSecurity detection
        if response_lower.contains("modsecurity") {
            candidates
                .entry("ModSecurity".to_string())
                .or_insert((0, vec![]))
                .0 += 60;
            candidates
                .entry("ModSecurity".to_string())
                .or_insert((0, vec![]))
                .1
                .push("ModSecurity header".to_string());
        }

        // Find best match
        if let Some((name, (confidence, indicators))) = candidates
            .into_iter()
            .max_by_key(|(_, (conf, _))| *conf)
        {
            if confidence > 40 {
                return Some(WAFDetection {
                    name,
                    detected: true,
                    confidence: std::cmp::min(confidence, 95),
                    indicators,
                });
            }
        }

        None
    }

    /// Format WAF detection for display
    pub fn format_waf(waf: &WAFDetection) -> String {
        format!(
            "WAF Detected: {} ({}% confidence)\nIndicators: {}",
            waf.name,
            waf.confidence,
            waf.indicators.join(", ")
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_waf_detection() {
        let response = "HTTP/1.1 200 OK\nCF-RAY: 123456\nContent-Type: text/html\n\n";
        let waf = WAFDetector::detect_waf(response);
        assert!(waf.is_some());
        if let Some(w) = waf {
            assert_eq!(w.name, "Cloudflare");
        }
    }

    #[test]
    fn test_no_waf() {
        let response = "HTTP/1.1 200 OK\nServer: Apache\n\n";
        let waf = WAFDetector::detect_waf(response);
        assert!(waf.is_none() || !waf.unwrap().detected);
    }
}
