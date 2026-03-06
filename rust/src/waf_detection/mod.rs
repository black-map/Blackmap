//! Web Application Firewall Detection module
//!
//! Identifies WAF presence via response analysis and headers.

/// Represents a detected WAF
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum WafProvider {
    Cloudflare,
    Imperva,
    AwsWaf,
    Akamai,
    Other(String),
}

/// Detects WAF presence based on HTTP response/banner
pub fn detect_waf(banner: &str) -> Option<WafProvider> {
    let lower_banner = banner.to_lowercase();
    
    if lower_banner.contains("cf-ray:") || lower_banner.contains("cloudflare-nginx") {
        return Some(WafProvider::Cloudflare);
    }
    
    if lower_banner.contains("x-iinfo:") || lower_banner.contains("incap_ses_") {
        return Some(WafProvider::Imperva);
    }
    
    if lower_banner.contains("x-amzn-requestid:") || lower_banner.contains("awselb") {
        return Some(WafProvider::AwsWaf);
    }
    
    if lower_banner.contains("akamai-ghost") || lower_banner.contains("x-akamai-request-id:") {
        return Some(WafProvider::Akamai);
    }
    
    None
}
