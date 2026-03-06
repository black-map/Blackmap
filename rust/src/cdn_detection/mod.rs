//! CDN Detection module
//!
//! Helper module to identify Content Delivery Networks through headers, ASNs, and IP ranges.

/// Represents a detected CDN
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CdnProvider {
    Cloudflare,
    Akamai,
    Fastly,
    AmazonCloudFront,
    Other(String),
}

/// Detects if the target is behind a CDN from the raw HTTP banner/response
pub fn detect_cdn(_ip: &str, banner: &str) -> Option<CdnProvider> {
    let lower_banner = banner.to_lowercase();
    
    if lower_banner.contains("server: cloudflare") || lower_banner.contains("cf-ray:") {
        return Some(CdnProvider::Cloudflare);
    }
    
    if lower_banner.contains("x-fastly-") || lower_banner.contains("fastly") {
        return Some(CdnProvider::Fastly);
    }
    
    if lower_banner.contains("x-amz-cf-id:") || lower_banner.contains("server: cloudfront") {
        return Some(CdnProvider::AmazonCloudFront);
    }
    
    if lower_banner.contains("x-akamai-") || lower_banner.contains("akamai") {
        return Some(CdnProvider::Akamai);
    }
    
    None
}
