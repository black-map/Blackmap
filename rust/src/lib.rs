use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use serde::{Deserialize, Serialize};
use regex::Regex;

#[derive(Serialize, Deserialize)]
struct ServiceInfo {
    service: String,
    version: Option<String>,
    banner: String,
    confidence: u8,
}

lazy_static::lazy_static! {
    static ref HTTP_REGEX: Regex = Regex::new(r"HTTP/([0-9.]+)").unwrap();
    static ref SSH_REGEX: Regex = Regex::new(r"SSH-([0-9.]+)").unwrap();
    static ref FTP_REGEX: Regex = Regex::new(r"220.*FTP").unwrap();
    static ref SMTP_REGEX: Regex = Regex::new(r"220.*SMTP").unwrap();
}

fn analyze_banner(banner: &str) -> ServiceInfo {
    let banner_lower = banner.to_lowercase();

    if banner_lower.contains("http") {
        let version = HTTP_REGEX.captures(banner).and_then(|cap| cap.get(1)).map(|m| m.as_str().to_string());
        ServiceInfo {
            service: "HTTP".to_string(),
            version,
            banner: banner.to_string(),
            confidence: 90,
        }
    } else if banner_lower.contains("ssh") {
        let version = SSH_REGEX.captures(banner).and_then(|cap| cap.get(1)).map(|m| m.as_str().to_string());
        ServiceInfo {
            service: "SSH".to_string(),
            version,
            banner: banner.to_string(),
            confidence: 95,
        }
    } else if FTP_REGEX.is_match(banner) {
        ServiceInfo {
            service: "FTP".to_string(),
            version: None, // Could extend to extract version
            banner: banner.to_string(),
            confidence: 85,
        }
    } else if SMTP_REGEX.is_match(banner) {
        ServiceInfo {
            service: "SMTP".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence: 80,
        }
    } else {
        ServiceInfo {
            service: "Unknown".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence: 0,
        }
    }
}

#[no_mangle]
pub extern "C" fn blackmap_analyze_banner(input: *const c_char) -> *const c_char {
    if input.is_null() {
        return std::ptr::null();
    }

    let c_str = unsafe { CStr::from_ptr(input) };
    let banner = match c_str.to_str() {
        Ok(s) => s,
        Err(_) => return std::ptr::null(),
    };

    let info = analyze_banner(banner);
    let json = match serde_json::to_string(&info) {
        Ok(j) => j,
        Err(_) => return std::ptr::null(),
    };

    let c_string = match CString::new(json) {
        Ok(c) => c,
        Err(_) => return std::ptr::null(),
    };

    c_string.into_raw()
}

#[no_mangle]
pub extern "C" fn blackmap_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe {
            let _ = CString::from_raw(s);
        }
    }
}