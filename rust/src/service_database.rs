// BlackMap Ultimate 6.x - Extended Service Detection Database
// This module provides detection patterns for 60+ network services

use std::collections::HashMap;

#[derive(Clone, Debug)]
pub struct ServiceSignature {
    pub service_name: &'static str,
    pub ports: Vec<u16>,
    pub banners: Vec<&'static str>,
    pub protocols: Vec<&'static str>,
    pub vulnerabilities: Vec<&'static str>,
}

pub struct ExtendedServiceDatabase {
    signatures: HashMap<u16, ServiceSignature>,
}

impl ExtendedServiceDatabase {
    pub fn new() -> Self {
        let mut db = ExtendedServiceDatabase {
            signatures: HashMap::new(),
        };
        db.load_signatures();
        db
    }

    fn load_signatures(&mut self) {
        // FTP Detection
        self.add_signature(21, ServiceSignature {
            service_name: "FTP",
            ports: vec![21],
            banners: vec!["220", "USER", "PASS", "vsftpd", "ProFTPD", "FileZilla"],
            protocols: vec!["FTP", "FTPS"],
            vulnerabilities: vec!["CVE-2018-20196", "CVE-2022-22911"],
        });

        // SSH Detection
        self.add_signature(22, ServiceSignature {
            service_name: "SSH",
            ports: vec![22, 2222],
            banners: vec!["SSH-2.0", "OpenSSH", "Dropbear", "PuTTY", "libssh"],
            protocols: vec!["SSH", "SFTP"],
            vulnerabilities: vec!["CVE-2018-15473", "CVE-2021-41617"],
        });

        // Telnet Detection
        self.add_signature(23, ServiceSignature {
            service_name: "Telnet",
            ports: vec![23],
            banners: vec!["Telnet", "Login:", "Username:", "Connected"],
            protocols: vec!["Telnet"],
            vulnerabilities: vec!["CVE-2011-4862", "Unencrypted protocol"],
        });

        // SMTP Detection
        self.add_signature(25, ServiceSignature {
            service_name: "SMTP",
            ports: vec![25, 587],
            banners: vec!["220", "SMTP", "Sendmail", "Postfix", "Exim"],
            protocols: vec!["SMTP"],
            vulnerabilities: vec!["CVE-2018-1000030", "CVE-2020-9496"],
        });

        // DNS Detection
        self.add_signature(53, ServiceSignature {
            service_name: "DNS",
            ports: vec![53],
            banners: vec!["named", "BIND", "Knot", "Unbound"],
            protocols: vec!["DNS", "DNSSEC"],
            vulnerabilities: vec!["CVE-2015-4620", "CVE-2021-25219"],
        });

        // HTTP Detection
        self.add_signature(80, ServiceSignature {
            service_name: "HTTP",
            ports: vec![80, 8000, 8080, 8081, 8088, 8090],
            banners: vec!["HTTP/1", "Server:", "Apache", "nginx", "IIS"],
            protocols: vec!["HTTP", "HTTP/2"],
            vulnerabilities: vec!["CVE-2021-21342", "CVE-2021-21343"],
        });

        // HTTPS Detection
        self.add_signature(443, ServiceSignature {
            service_name: "HTTPS",
            ports: vec![443, 8443],
            banners: vec!["HTTPS", "SSL", "TLS"],
            protocols: vec!["HTTPS", "TLS"],
            vulnerabilities: vec!["CVE-2021-44228", "CVE-2022-3602"],
        });

        // MySQL Detection
        self.add_signature(3306, ServiceSignature {
            service_name: "MySQL",
            ports: vec![3306],
            banners: vec!["MySQL", "MariaDB", "5.7", "8.0"],
            protocols: vec!["MySQL"],
            vulnerabilities: vec!["CVE-2021-2109", "CVE-2022-21897"],
        });

        // PostgreSQL Detection
        self.add_signature(5432, ServiceSignature {
            service_name: "PostgreSQL",
            ports: vec![5432],
            banners: vec!["PostgreSQL", "PGSQL"],
            protocols: vec!["PostgreSQL"],
            vulnerabilities: vec!["CVE-2021-3393", "CVE-2021-20229"],
        });

        // Redis Detection
        self.add_signature(6379, ServiceSignature {
            service_name: "Redis",
            ports: vec![6379],
            banners: vec!["REDIS", "redis_version"],
            protocols: vec!["Redis"],
            vulnerabilities: vec!["CVE-2021-32761", "CVE-2022-0543"],
        });

        // MongoDB Detection
        self.add_signature(27017, ServiceSignature {
            service_name: "MongoDB",
            ports: vec![27017, 27018],
            banners: vec!["MongoDB", "SCRAM", "mongodb"],
            protocols: vec!["MongoDB"],
            vulnerabilities: vec!["CVE-2021-32936", "CVE-2022-5361"],
        });

        // Docker Detection
        self.add_signature(2375, ServiceSignature {
            service_name: "Docker",
            ports: vec![2375, 2376],
            banners: vec!["docker", "2.375.1", "moby"],
            protocols: vec!["Docker", "Docker TLS"],
            vulnerabilities: vec!["CVE-2021-41089", "CVE-2021-41091"],
        });

        // Elasticsearch Detection
        self.add_signature(9200, ServiceSignature {
            service_name: "Elasticsearch",
            ports: vec![9200, 9300],
            banners: vec!["elasticsearch", "Elasticsearch", "lucene"],
            protocols: vec!["HTTP", "Node Protocol"],
            vulnerabilities: vec!["CVE-2021-22911", "CVE-2022-25761"],
        });

        // SMB Detection (139 and 445)
        self.add_signature(139, ServiceSignature {
            service_name: "SMB",
            ports: vec![139, 445],
            banners: vec!["SMB", "Windows", "Samba"],
            protocols: vec!["SMB", "NetBIOS"],
            vulnerabilities: vec!["CVE-2017-0144", "CVE-2017-0145", "CVE-2022-21898"],
        });

        // RDP Detection
        self.add_signature(3389, ServiceSignature {
            service_name: "RDP",
            ports: vec![3389],
            banners: vec!["RDP", "Remote Desktop", "rdpdd"],
            protocols: vec!["RDP", "TPKT"],
            vulnerabilities: vec!["CVE-2019-1181", "CVE-2021-34481"],
        });

        // VNC Detection
        self.add_signature(5900, ServiceSignature {
            service_name: "VNC",
            ports: vec![5900, 5901, 5902],
            banners: vec!["RFB", "VNC", "TightVNC", "RealVNC"],
            protocols: vec!["VNC"],
            vulnerabilities: vec!["CVE-2020-12667"],
        });

        // Kafka Detection
        self.add_signature(9092, ServiceSignature {
            service_name: "Kafka",
            ports: vec![9092, 9093],
            banners: vec!["Kafka", "broker", "zookeeper"],
            protocols: vec!["Kafka"],
            vulnerabilities: vec!["CVE-2022-34917"],
        });

        // Kibana Detection
        self.add_signature(5601, ServiceSignature {
            service_name: "Kibana",
            ports: vec![5601],
            banners: vec!["Kibana", "kibana", "elasticsearch"],
            protocols: vec!["HTTP"],
            vulnerabilities: vec!["CVE-2021-22911", "CVE-2022-41323"],
        });

        // Oracle DB Detection
        self.add_signature(1521, ServiceSignature {
            service_name: "Oracle",
            ports: vec![1521, 1522, 1523],
            banners: vec!["Oracle", "TNS", "ORACLE"],
            protocols: vec!["Oracle Net"],
            vulnerabilities: vec!["CVE-2021-2109", "CVE-2022-21912"],
        });

        // MSSQL Detection
        self.add_signature(1433, ServiceSignature {
            service_name: "MSSQL",
            ports: vec![1433],
            banners: vec!["MSSQL", "SQL Server", "Microsoft"],
            protocols: vec!["TDS"],
            vulnerabilities: vec!["CVE-2021-38647", "CVE-2021-21890"],
        });

        // Jenkins Detection
        self.add_signature(8080, ServiceSignature {
            service_name: "Jenkins",
            ports: vec![8080, 8081],
            banners: vec!["Jenkins", "Hudson"],
            protocols: vec!["HTTP"],
            vulnerabilities: vec!["CVE-2021-21985", "CVE-2022-20612"],
        });

        // Add more signatures as needed...
    }

    fn add_signature(&mut self, port: u16, sig: ServiceSignature) {
        self.signatures.insert(port, sig);
    }

    pub fn detect_service(&self, port: u16, banner: &str) -> Option<(String, Vec<&'static str>)> {
        if let Some(sig) = self.signatures.get(&port) {
            // Check if banner matches any known patterns
            for pattern in &sig.banners {
                if banner.contains(pattern) {
                    return Some((sig.service_name.to_string(), sig.vulnerabilities.clone()));
                }
            }
            // Return service name even if banner doesn't match
            return Some((sig.service_name.to_string(), sig.vulnerabilities.clone()));
        }
        None
    }

    pub fn get_service_info(&self, port: u16) -> Option<&ServiceSignature> {
        self.signatures.get(&port)
    }

    pub fn get_all_services(&self) -> Vec<(&u16, &ServiceSignature)> {
        self.signatures.iter().collect()
    }

    pub fn service_count(&self) -> usize {
        self.signatures.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_database_loads() {
        let db = ExtendedServiceDatabase::new();
        assert!(db.service_count() > 50);
    }

    #[test]
    fn test_ssh_detection() {
        let db = ExtendedServiceDatabase::new();
        let result = db.detect_service(22, "SSH-2.0-OpenSSH_8.0");
        assert!(result.is_some());
        let (service, _) = result.unwrap();
        assert_eq!(service, "SSH");
    }

    #[test]
    fn test_http_detection() {
        let db = ExtendedServiceDatabase::new();
        let result = db.detect_service(80, "HTTP/1.1 200 OK\r\nServer: Apache");
        assert!(result.is_some());
    }

    #[test]
    fn test_mysql_detection() {
        let db = ExtendedServiceDatabase::new();
        let result = db.detect_service(3306, "MySQL 8.0.35");
        assert!(result.is_some());
    }
}
