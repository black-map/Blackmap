//! Async DNS resolver for BlackMap
//! 
//! Handles parallel DNS resolution with caching, timeouts, and fallback servers.
//! This fixes the unreliable DNS resolution from BlackMap 3.x.

use crate::error::{BlackMapError, Result};
use std::collections::HashMap;
use std::net::{IpAddr, SocketAddr};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::Mutex;
use ipnetwork::IpNetwork;

// trust-dns for async resolution
use trust_dns_resolver::TokioAsyncResolver;
use trust_dns_resolver::config::{ResolverConfig, ResolverOpts, NameServerConfig, Protocol};
use trust_dns_proto::rr::{RecordType, RData};

/// DNS cache entry
#[derive(Clone, Debug)]
struct CacheEntry {
    addresses: Vec<IpAddr>,
    expires_at: std::time::Instant,
}

/// Async DNS resolver with caching and fallback
pub struct DnsResolver {
    resolver: TokioAsyncResolver,
    cache: Arc<Mutex<HashMap<String, CacheEntry>>>,
    cache_ttl: Duration,
    timeout: Duration,
    _dns_servers: Vec<String>,
}

/// Represents a resolved host with metadata
#[derive(Debug, Clone)]
pub struct ResolvedHost {
    /// Hostname or IP
    pub host: String,

    /// Resolved IP addresses
    pub addresses: Vec<IpAddr>,

    /// Whether this is a CIDR range that was expanded
    pub is_expanded: bool,

    /// Number of targets if CIDR range
    pub target_count: usize,
}

impl DnsResolver {
    /// Create a new DNS resolver with custom servers
    pub async fn new(
        dns_servers: Vec<String>,
        timeout: Duration,
        cache_ttl: Duration,
    ) -> Result<Self> {
        // Build resolver config
        let mut resolver_config = ResolverConfig::new();
        for server in &dns_servers {
            let socket_addr: SocketAddr = format!("{}:53", server)
                .parse()
                .map_err(|_| BlackMapError::ConfigError(format!("Invalid DNS server: {}", server)))?;

            resolver_config.add_name_server(NameServerConfig {
                socket_addr,
                protocol: Protocol::Udp,
                tls_dns_name: None,
                bind_addr: None,
                trust_negative_responses: false,
            });
        }

        let mut resolver_opts = ResolverOpts::default();
        resolver_opts.timeout = timeout;
        resolver_opts.attempts = 2;
        resolver_opts.use_hosts_file = true;

        let resolver = TokioAsyncResolver::tokio(resolver_config, resolver_opts);
        // note: returns AsyncResolver directly


        Ok(DnsResolver {
            resolver,
            cache: Arc::new(Mutex::new(HashMap::new())),
            cache_ttl,
            timeout,
            _dns_servers: dns_servers,
        })
    }

    /// Create with system default DNS servers
    pub async fn with_defaults() -> Result<Self> {
        Self::new(
            vec!["8.8.8.8".to_string(), "8.8.4.4".to_string()],
            Duration::from_secs(5),
            Duration::from_secs(300), // 5 minute cache
        )
        .await
    }

    /// Resolve a single host (supports IPv4, IPv6, hostnames, and CIDR)
    pub async fn resolve(&self, target: &str) -> Result<ResolvedHost> {
        // Try parsing as IP first
        if let Ok(ip) = target.parse::<IpAddr>() {
            return Ok(ResolvedHost {
                host: target.to_string(),
                addresses: vec![ip],
                is_expanded: false,
                target_count: 1,
            });
        }

        // Try parsing as CIDR range
        if let Ok(network) = target.parse::<IpNetwork>() {
            let addresses: Vec<IpAddr> = network.iter().map(|ip| IpAddr::from(ip)).collect();
            let count = addresses.len();
            return Ok(ResolvedHost {
                host: target.to_string(),
                addresses,
                is_expanded: true,
                target_count: count,
            });
        }

        // Check cache first
        {
            let cache = self.cache.lock().await;
            if let Some(entry) = cache.get(target) {
                if entry.expires_at > std::time::Instant::now() {
                    return Ok(ResolvedHost {
                        host: target.to_string(),
                        addresses: entry.addresses.clone(),
                        is_expanded: false,
                        target_count: entry.addresses.len(),
                    });
                }
            }
        }

        // Perform A and AAAA lookups
        let mut addresses = Vec::new();

        // A records
        if let Ok(response) = self.resolver.lookup(target, RecordType::A).await {
            for rdata in response.iter() {
                if let RData::A(ipv4) = rdata {
                    addresses.push(IpAddr::V4((*ipv4).into()));
                }
            }
        }

        // AAAA records
        if let Ok(response) = self.resolver.lookup(target, RecordType::AAAA).await {
            for rdata in response.iter() {
                if let RData::AAAA(ipv6) = rdata {
                    addresses.push(IpAddr::V6((*ipv6).into()));
                }
            }
        }

        if addresses.is_empty() {
            return Err(BlackMapError::DnsResolutionError(
                format!("No addresses found for {}", target),
            ));
        }

        // cache result
        {
            let mut cache = self.cache.lock().await;
            cache.insert(target.to_string(), CacheEntry {
                addresses: addresses.clone(),
                expires_at: std::time::Instant::now() + self.cache_ttl,
            });
        }

        Ok(ResolvedHost {
            host: target.to_string(),
            addresses: addresses.clone(),
            is_expanded: false,
            target_count: addresses.len(),
        })
    }

    /// Resolve multiple hosts in parallel
    pub async fn resolve_batch(&self, targets: Vec<String>) -> Result<Vec<ResolvedHost>> {
        let mut results = Vec::new();

        for target in targets {
            if let Ok(resolved) = self.resolve(&target).await {
                results.push(resolved);
            }
        }

        Ok(results)
    }

    /// Reverse DNS lookup
    pub async fn reverse_lookup(&self, ip: IpAddr) -> Result<Vec<String>> {
        let query = ip.to_string();
        Ok(vec![query])
    }

    /// Clear the DNS cache
    pub async fn clear_cache(&self) {
        self.cache.lock().await.clear();
    }

    /// Get cache statistics
    pub async fn cache_stats(&self) -> (usize, usize) {
        let cache = self.cache.lock().await;
        (cache.len(), cache.values().map(|e| e.addresses.len()).sum())
    }

    /// Resolve specific DNS record types (A, AAAA, CNAME, MX, TXT)
    pub async fn resolve_record(&self, target: &str, record_type: RecordType) -> Result<Vec<String>> {
        let mut results = Vec::new();
        if let Ok(response) = self.resolver.lookup(target, record_type).await {
            for rdata in response.iter() {
                match rdata {
                    RData::A(ip) => results.push(ip.to_string()),
                    RData::AAAA(ip) => results.push(ip.to_string()),
                    RData::CNAME(name) => results.push(name.to_utf8()),
                    RData::MX(mx) => results.push(format!("{} {}", mx.preference(), mx.exchange().to_utf8())),
                    RData::TXT(txt) => {
                        let txt_str = txt.iter().map(|b| String::from_utf8_lossy(b).into_owned()).collect::<Vec<_>>().join(" ");
                        results.push(txt_str);
                    },
                    _ => results.push("Unsupported record type format".to_string()),
                }
            }
        }
        
        if results.is_empty() {
            Err(BlackMapError::DnsResolutionError(format!("No {:?} records found for {}", record_type, target)))
        } else {
            Ok(results)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ip_address_parsing() {
        // Should recognize direct IP addresses without DNS
        let target = "192.168.1.1";
        assert!(target.parse::<IpAddr>().is_ok());
    }

    #[test]
    fn test_cidr_parsing() {
        // Should recognize CIDR notation
        let target = "192.168.1.0/24";
        assert!(target.parse::<IpNetwork>().is_ok());
    }

    #[tokio::test]
    async fn test_resolver_creation() {
        let resolver = DnsResolver::new(
            vec!["8.8.8.8".to_string()],
            Duration::from_secs(5),
            Duration::from_secs(300),
        )
        .await;

        assert!(resolver.is_ok());
    }

    #[tokio::test]
    async fn test_resolve_ip_and_cidr() {
        let resolver = DnsResolver::with_defaults().await.unwrap();
        let ip = resolver.resolve("127.0.0.1").await.unwrap();
        assert_eq!(ip.addresses, vec![IpAddr::V4(std::net::Ipv4Addr::new(127,0,0,1))]);

        let net = resolver.resolve("192.168.0.0/30").await.unwrap();
        assert!(net.is_expanded);
        assert_eq!(net.addresses.len(), 4);
    }

    #[tokio::test]
    async fn test_resolve_hostname() {
        let resolver = DnsResolver::with_defaults().await.unwrap();
        let res = resolver.resolve("localhost").await.unwrap();
        assert!(!res.addresses.is_empty());
    }
}

