//! Foreign Function Interface bindings for Blackmap 4.0 C engines
use libc::{sockaddr_storage, socklen_t};

// ----------------------------------------------------------------------------
// DISCOVERY.H
// ----------------------------------------------------------------------------
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum DiscoveryMethod {
    None = 0,
    IcmpEcho = 1,
    TcpSyn = 2,
    TcpAck = 3,
    TcpConnect = 4,
    Udp = 5,
    Combined = 6,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum DiscoveryProbeType {
    Icmp = 0,
    TcpSyn = 1,
    TcpAck = 2,
    TcpConnect = 3,
    Udp = 4,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct DiscoveryStats {
    pub total_probes_sent: u32,
    pub successful_probes: u32,
    pub failed_probes: u32,
    pub timeouts: u32,
    pub hosts_discovered_up: u32,
    pub duration_ms: u32,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct DiscoveryResult {
    pub addr: sockaddr_storage,
    pub addr_len: socklen_t,
    pub family: std::os::raw::c_int,
    pub addr_str: [std::os::raw::c_char; 46],
    pub hostname: [std::os::raw::c_char; 256],
    pub is_up: bool,
    pub rtt_ms: u32,
    pub probe_method_used: DiscoveryProbeType,
}

#[repr(C)]
#[derive(Debug, Clone)]
pub struct DiscoveryConfig {
    pub method: DiscoveryMethod,
    pub probe_ports: *mut u16,
    pub probe_port_count: u16,
    pub timeout_ms: u32,
    pub max_retries: u32,
    pub skip_discovery: bool,
    pub verbose: bool,
}

extern "C" {
    pub fn discovery_config_create() -> *mut DiscoveryConfig;
    pub fn discovery_config_free(config: *mut DiscoveryConfig);
    pub fn discovery_probe_host(
        config: *const DiscoveryConfig,
        target: *mut sockaddr_storage,
        result: *mut DiscoveryResult,
    ) -> std::os::raw::c_int;
}
