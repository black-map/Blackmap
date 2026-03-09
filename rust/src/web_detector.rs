//! Web Technology Detection - Identifies web servers, frameworks, CMS, etc.
//! Detects: Apache, Nginx, IIS, PHP, ASP.NET, WordPress, Drupal, Joomla, etc.

use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;

#[derive(Debug, Clone)]
pub struct WebTechnology {
    pub category: String,      // "Server", "Language", "CMS", "Framework"
    pub name: String,
    pub version: Option<String>,
    pub confidence: u8,
}

pub struct WebDetector;

impl WebDetector {
    /// Perform web technology detection on a target
    pub fn detect_technologies(host: &str, port: u16) -> Vec<WebTechnology> {
        let mut techs = Vec::new();

        match TcpStream::connect(format!("{}:{}", host, port)) {
            Ok(mut stream) => {
                stream.set_read_timeout(Some(Duration::from_secs(5))).ok();
                stream.set_write_timeout(Some(Duration::from_secs(5))).ok();

                // Send HTTP request
                let request = format!(
                    "GET / HTTP/1.1\r\nHost: {}\r\nConnection: close\r\nUser-Agent: BlackMap/6.1\r\n\r\n",
                    host
                );

                if stream.write_all(request.as_bytes()).is_ok() {
                    let mut response = String::new();
                    let mut buffer = vec![0u8; 8192];

                    if let Ok(n) = stream.read(&mut buffer) {
                        if let Ok(response_str) = String::from_utf8(buffer[..n].to_vec()) {
                            response = response_str;
                        }
                    }

                    // Parse technologies
                    techs.extend(Self::detect_server(&response));
                    techs.extend(Self::detect_language(&response));
                    techs.extend(Self::detect_cms(&response));
                    techs.extend(Self::detect_frameworks(&response));
                    techs.extend(Self::detect_cookies(&response));
                }
            }
            Err(_) => {}
        }

        techs
    }

    /// Detect web server
    fn detect_server(response: &str) -> Vec<WebTechnology> {
        let mut techs = Vec::new();

        let lines: Vec<&str> = response.lines().collect();
        for line in lines.iter().take(20) {
            if line.starts_with("Server:") {
                let server = line.replace("Server:", "").trim().to_string();

                if server.contains("Microsoft-IIS") {
                    techs.push(WebTechnology {
                        category: "Server".to_string(),
                        name: "Microsoft IIS".to_string(),
                        version: server.split('/').nth(1).map(|s| s.to_string()),
                        confidence: 95,
                    });
                } else if server.contains("Apache") {
                    techs.push(WebTechnology {
                        category: "Server".to_string(),
                        name: "Apache".to_string(),
                        version: server.split('/').nth(1).map(|s| s.to_string()),
                        confidence: 95,
                    });
                } else if server.contains("nginx") {
                    techs.push(WebTechnology {
                        category: "Server".to_string(),
                        name: "Nginx".to_string(),
                        version: server.split('/').nth(1).map(|s| s.to_string()),
                        confidence: 95,
                    });
                } else if server.contains("LiteSpeed") {
                    techs.push(WebTechnology {
                        category: "Server".to_string(),
                        name: "LiteSpeed".to_string(),
                        version: server.split('/').nth(1).map(|s| s.to_string()),
                        confidence: 90,
                    });
                }
                break;
            }
        }

        techs
    }

    /// Detect programming language/framework
    fn detect_language(response: &str) -> Vec<WebTechnology> {
        let mut techs = Vec::new();

        let response_lower = response.to_lowercase();

        if response_lower.contains("x-powered-by: php") || response_lower.contains(".php") {
            techs.push(WebTechnology {
                category: "Language".to_string(),
                name: "PHP".to_string(),
                version: None,
                confidence: 85,
            });
        }

        if response_lower.contains("x-aspnet-version") || response_lower.contains("asp.net") {
            techs.push(WebTechnology {
                category: "Framework".to_string(),
                name: "ASP.NET".to_string(),
                version: None,
                confidence: 90,
            });
        }

        if response_lower.contains("x-powered-by: express") {
            techs.push(WebTechnology {
                category: "Framework".to_string(),
                name: "Express.js".to_string(),
                version: None,
                confidence: 85,
            });
        }

        techs
    }

    /// Detect CMS
    fn detect_cms(response: &str) -> Vec<WebTechnology> {
        let mut techs = Vec::new();
        let response_lower = response.to_lowercase();

        if response_lower.contains("wp-content") || response_lower.contains("wordpress") {
            techs.push(WebTechnology {
                category: "CMS".to_string(),
                name: "WordPress".to_string(),
                version: None,
                confidence: 95,
            });
        }

        if response_lower.contains("drupal") || response_lower.contains("sites/default") {
            techs.push(WebTechnology {
                category: "CMS".to_string(),
                name: "Drupal".to_string(),
                version: None,
                confidence: 90,
            });
        }

        if response_lower.contains("joomla") {
            techs.push(WebTechnology {
                category: "CMS".to_string(),
                name: "Joomla".to_string(),
                version: None,
                confidence: 90,
            });
        }

        techs
    }

    /// Detect frameworks
    fn detect_frameworks(response: &str) -> Vec<WebTechnology> {
        let mut techs = Vec::new();
        let response_lower = response.to_lowercase();

        if response_lower.contains("rails") {
            techs.push(WebTechnology {
                category: "Framework".to_string(),
                name: "Ruby on Rails".to_string(),
                version: None,
                confidence: 80,
            });
        }

        if response_lower.contains("django") {
            techs.push(WebTechnology {
                category: "Framework".to_string(),
                name: "Django".to_string(),
                version: None,
                confidence: 85,
            });
        }

        techs
    }

    /// Detect from cookies
    fn detect_cookies(response: &str) -> Vec<WebTechnology> {
        let mut techs = Vec::new();

        for line in response.lines() {
            if line.contains("Set-Cookie:") {
                let cookie_lower = line.to_lowercase();

                if cookie_lower.contains("jsessionid") {
                    techs.push(WebTechnology {
                        category: "Language".to_string(),
                        name: "Java".to_string(),
                        version: None,
                        confidence: 70,
                    });
                } else if cookie_lower.contains("phpsessid") {
                    techs.push(WebTechnology {
                        category: "Language".to_string(),
                        name: "PHP".to_string(),
                        version: None,
                        confidence: 80,
                    });
                } else if cookie_lower.contains("aspsessionid") {
                    techs.push(WebTechnology {
                        category: "Framework".to_string(),
                        name: "ASP".to_string(),
                        version: None,
                        confidence: 85,
                    });
                }
            }
        }

        techs
    }

    /// Format technologies for display
    pub fn format_technologies(techs: &[WebTechnology]) -> String {
        if techs.is_empty() {
            return "No web technologies detected".to_string();
        }

        let mut output = String::from("Web Technologies Detected:\n\n");
        for tech in techs {
            let version_str = tech
                .version
                .as_ref()
                .map(|v| format!(" {}", v))
                .unwrap_or_default();
            output.push_str(&format!(
                "{}: {}{}  ({}% confidence)\n",
                tech.category, tech.name, version_str, tech.confidence
            ));
        }
        output
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_technology_creation() {
        let tech = WebTechnology {
            category: "Server".to_string(),
            name: "Nginx".to_string(),
            version: Some("1.20.0".to_string()),
            confidence: 95,
        };
        assert_eq!(tech.name, "Nginx");
        assert_eq!(tech.confidence, 95);
    }

    #[test]
    fn test_format_technologies() {
        let techs = vec![WebTechnology {
            category: "Server".to_string(),
            name: "Apache".to_string(),
            version: Some("2.4.41".to_string()),
            confidence: 95,
        }];
        let output = WebDetector::format_technologies(&techs);
        assert!(output.contains("Apache"));
    }
}
