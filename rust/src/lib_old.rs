use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use serde::{Deserialize, Serialize};
use regex::Regex;

#[derive(Serialize, Deserialize, Debug)]
struct ServiceInfo {
    service: String,
    version: Option<String>,
    banner: String,
    confidence: u8,
    #[serde(skip_serializing_if = "Option::is_none")]
    extra_fields: Option<std::collections::HashMap<String, String>>,
}

lazy_static::lazy_static! {
    // HTTP patterns
    static ref HTTP_REGEX: Regex = Regex::new(r"HTTP/([0-9.]+)").unwrap();
    static ref HTTP_SERVER: Regex = Regex::new(r"Server:\s*([^\r\n]+)").unwrap();
    static ref APACHE_REGEX: Regex = Regex::new(r"Apache[/\s]+([0-9.]+)").unwrap();
    static ref NGINX_REGEX: Regex = Regex::new(r"nginx[/\s]+([0-9.]+)").unwrap();
    static ref IIS_REGEX: Regex = Regex::new(r"IIS[/\s]+([0-9.]+)").unwrap();

    // SSH patterns
    static ref SSH_REGEX: Regex = Regex::new(r"SSH-([0-9.]+)-(.+)").unwrap();
    static ref OPENSSH_REGEX: Regex = Regex::new(r"OpenSSH[_\s]+([0-9.]+)").unwrap();
    static ref LIBSSH_REGEX: Regex = Regex::new(r"libssh[_\s]+([0-9.]+)").unwrap();

    // FTP patterns
    static ref FTP_REGEX: Regex = Regex::new(r"220.*FTP").unwrap();
    static ref VSFTPD_REGEX: Regex = Regex::new(r"vsftpd\s+([0-9.]+)").unwrap();
    static ref PROFTPD_REGEX: Regex = Regex::new(r"ProFTPD\s+([0-9.]+)").unwrap();

    // SMTP patterns
    static ref SMTP_REGEX: Regex = Regex::new(r"220\s+.*SMTP").unwrap();
    static ref POSTFIX_REGEX: Regex = Regex::new(r"Postfix").unwrap();
    static ref SENDMAIL_REGEX: Regex = Regex::new(r"Sendmail").unwrap();

    // Database patterns
    static ref MYSQL_REGEX: Regex = Regex::new(r"([0-9]+\.[0-9]+\.[0-9]+[a-zA-Z0-9\-]*)-MySQL").unwrap();
    static ref POSTGRES_REGEX: Regex = Regex::new(r"PostgreSQL\s+([0-9.]+)").unwrap();
    static ref MONGODB_REGEX: Regex = Regex::new(r"MongoDB\s+([0-9.]+)").unwrap();
    static ref REDIS_REGEX: Regex = Regex::new(r"redis_version:([0-9.]+)").unwrap();

    // Other services
    static ref DNS_REGEX: Regex = Regex::new(r"(?i)(bind|dnsmasq|PowerDNS|CoreDNS)").unwrap();
    static ref TELNET_REGEX: Regex = Regex::new(r"(?i)(telnet|network.*protocol)").unwrap();
}

fn analyze_banner(banner: &str) -> ServiceInfo {
    let banner_lower = banner.to_lowercase();
    let mut extra_fields = std::collections::HashMap::new();

    // SSH Detection (highest priority)
    if banner.starts_with("SSH-") {
        if let Some(caps) = SSH_REGEX.captures(banner) {
            let version = caps.get(1).map(|m| m.as_str().to_string());
            let impl_name = caps.get(2).map(|m| m.as_str().to_string());
            
            let mut confidence = 99;
            let mut detected_version = version.clone();
            
            if let Some(ref impl_) = impl_name {
                extra_fields.insert("implementation".to_string(), impl_.clone());
                
                if OPENSSH_REGEX.is_match(impl_) {
                    if let Some(cv) = OPENSSH_REGEX.captures(impl_).and_then(|c| c.get(1)) {
                        detected_version = Some(cv.as_str().to_string());
                    }
                    extra_fields.insert("product".to_string(), "OpenSSH".to_string());
                } else if LIBSSH_REGEX.is_match(impl_) {
                    extra_fields.insert("product".to_string(), "libssh".to_string());
                    confidence = 95;
                }
            }
            
            return ServiceInfo {
                service: "SSH".to_string(),
                version: detected_version,
                banner: banner.to_string(),
                confidence,
                extra_fields: if extra_fields.is_empty() { None } else { Some(extra_fields) },
            };
        }
    }

    // HTTP Detection
    if banner_lower.contains("http/") {
        let version = HTTP_REGEX.captures(banner).and_then(|cap| cap.get(1)).map(|m| m.as_str().to_string());
        let mut confidence = 95;
        let mut server_info = String::new();

        if let Some(caps) = HTTP_SERVER.captures(banner) {
            if let Some(server) = caps.get(1) {
                server_info = server.as_str().to_string();
                extra_fields.insert("server".to_string(), server_info.clone());

                // Detect web server type and extract detailed version
                if let Some(apache_ver) = APACHE_REGEX.captures(&server_info).and_then(|c| c.get(1)) {
                    extra_fields.insert("product".to_string(), "Apache".to_string());
                    return ServiceInfo {
                        service: "HTTP".to_string(),
                        version: Some(apache_ver.as_str().to_string()),
                        banner: banner.to_string(),
                        confidence: 98,
                        extra_fields: Some(extra_fields),
                    };
                } else if let Some(nginx_ver) = NGINX_REGEX.captures(&server_info).and_then(|c| c.get(1)) {
                    extra_fields.insert("product".to_string(), "nginx".to_string());
                    return ServiceInfo {
                        service: "HTTP".to_string(),
                        version: Some(nginx_ver.as_str().to_string()),
                        banner: banner.to_string(),
                        confidence: 98,
                        extra_fields: Some(extra_fields),
                    };
                } else if IIS_REGEX.is_match(&server_info) {
                    extra_fields.insert("product".to_string(), "Microsoft IIS".to_string());
                    confidence = 96;
                }
            }
        }

        return ServiceInfo {
            service: "HTTP".to_string(),
            version,
            banner: banner.to_string(),
            confidence,
            extra_fields: if extra_fields.is_empty() { None } else { Some(extra_fields) },
        };
    }

    // FTP Detection
    if FTP_REGEX.is_match(banner) {
        let mut confidence = 85;
        if let Some(caps) = VSFTPD_REGEX.captures(banner).and_then(|c| c.get(1)) {
            extra_fields.insert("product".to_string(), "vsftpd".to_string());
            return ServiceInfo {
                service: "FTP".to_string(),
                version: Some(caps.as_str().to_string()),
                banner: banner.to_string(),
                confidence: 92,
                extra_fields: Some(extra_fields),
            };
        } else if let Some(caps) = PROFTPD_REGEX.captures(banner).and_then(|c| c.get(1)) {
            extra_fields.insert("product".to_string(), "ProFTPD".to_string());
            return ServiceInfo {
                service: "FTP".to_string(),
                version: Some(caps.as_str().to_string()),
                banner: banner.to_string(),
                confidence: 92,
                extra_fields: Some(extra_fields),
            };
        }
        
        return ServiceInfo {
            service: "FTP".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence,
            extra_fields: None,
        };
    }

    // SMTP Detection
    if SMTP_REGEX.is_match(banner) {
        let mut confidence = 85;
        let mut product = String::new();

        if POSTFIX_REGEX.is_match(banner) {
            product = "Postfix".to_string();
            confidence = 90;
        } else if SENDMAIL_REGEX.is_match(banner) {
            product = "Sendmail".to_string();
            confidence = 88;
        }

        if !product.is_empty() {
            extra_fields.insert("product".to_string(), product);
        }

        return ServiceInfo {
            service: "SMTP".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence,
            extra_fields: if extra_fields.is_empty() { None } else { Some(extra_fields) },
        };
    }

    // MySQL Detection
    if let Some(caps) = MYSQL_REGEX.captures(banner).and_then(|c| c.get(1)) {
        return ServiceInfo {
            service: "MySQL".to_string(),
            version: Some(caps.as_str().to_string()),
            banner: banner.to_string(),
            confidence: 98,
            extra_fields: None,
        };
    }

    // PostgreSQL Detection
    if let Some(caps) = POSTGRES_REGEX.captures(banner).and_then(|c| c.get(1)) {
        return ServiceInfo {
            service: "PostgreSQL".to_string(),
            version: Some(caps.as_str().to_string()),
            banner: banner.to_string(),
            confidence: 96,
            extra_fields: None,
        };
    }

    // MongoDB Detection
    if let Some(caps) = MONGODB_REGEX.captures(banner).and_then(|c| c.get(1)) {
        return ServiceInfo {
            service: "MongoDB".to_string(),
            version: Some(caps.as_str().to_string()),
            banner: banner.to_string(),
            confidence: 95,
            extra_fields: None,
        };
    }

    // Redis Detection
    if let Some(caps) = REDIS_REGEX.captures(banner).and_then(|c| c.get(1)) {
        return ServiceInfo {
            service: "Redis".to_string(),
            version: Some(caps.as_str().to_string()),
            banner: banner.to_string(),
            confidence: 95,
            extra_fields: None,
        };
    }

    // DNS Detection
    if DNS_REGEX.is_match(banner) {
        return ServiceInfo {
            service: "DNS".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence: 85,
            extra_fields: None,
        };
    }

    // Telnet Detection
    if TELNET_REGEX.is_match(banner) {
        return ServiceInfo {
            service: "Telnet".to_string(),
            version: None,
            banner: banner.to_string(),
            confidence: 70,
            extra_fields: None,
        };
    }

    // Unknown service
    ServiceInfo {
        service: "Unknown".to_string(),
        version: None,
        banner: banner.to_string(),
        confidence: 0,
        extra_fields: None,
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