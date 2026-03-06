//! Subdomain Enumeration module
//!
//! Performs DNS brute-forcing to discover subdomains.

use crate::dns::DnsResolver;
use crate::error::Result;
use std::sync::Arc;
use tokio::sync::mpsc;
use tokio::task;

const DEFAULT_WORDLIST: &[&str] = &[
    "www", "mail", "api", "dev", "test", "admin", "beta", "staging",
    "prod", "vpn", "portal", "ns1", "ns2", "smtp", "pop", "imap",
    "ftp", "ssh", "git", "shop", "blog", "app", "cdn", "db", "auth"
];

/// Result of a successful subdomain resolution
#[derive(Debug, Clone)]
pub struct SubdomainResult {
    pub subdomain: String,
    pub ips: Vec<std::net::IpAddr>,
}

/// Enumerates subdomains for a given target domain using concurrent DNS resolution
pub async fn enumerate_subdomains(domain: &str, resolver: Arc<DnsResolver>, threads: usize) -> Result<Vec<SubdomainResult>> {
    let (tx, mut rx) = mpsc::channel(100);
    
    // Generator task
    let target_domain = domain.to_string();
    let producer = tokio::spawn(async move {
        for &word in DEFAULT_WORDLIST {
            let subdomain = format!("{}.{}", word, target_domain);
            if tx.send(subdomain).await.is_err() {
                break;
            }
        }
    });

    let mut workers = Vec::new();
    let rx = Arc::new(tokio::sync::Mutex::new(rx));

    for _ in 0..threads {
        let resolver_clone = Arc::clone(&resolver);
        let rx_clone = Arc::clone(&rx);
        
        // Consumer task
        let worker = tokio::spawn(async move {
            let mut local_results = Vec::new();
            loop {
                let subdomain = {
                    let mut rx_lock = rx_clone.lock().await;
                    rx_lock.recv().await
                };

                match subdomain {
                    Some(sub) => {
                        // Attempt to resolve A and AAAA records via standard resolve
                        if let Ok(resolved) = resolver_clone.resolve(&sub).await {
                            if !resolved.addresses.is_empty() {
                                local_results.push(SubdomainResult {
                                    subdomain: sub,
                                    ips: resolved.addresses,
                                });
                            }
                        }
                    }
                    None => break, // Channel closed
                }
            }
            local_results
        });
        workers.push(worker);
    }

    producer.await.ok();

    let mut final_results = Vec::new();
    for worker in workers {
        if let Ok(mut res) = worker.await {
            final_results.append(&mut res);
        }
    }

    Ok(final_results)
}
