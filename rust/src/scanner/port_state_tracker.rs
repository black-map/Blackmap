use std::net::IpAddr;
use dashmap::DashMap;
use crate::scanner::PortState;
use std::time::{Instant, Duration};
use std::sync::atomic::{AtomicUsize, Ordering};

/// Tracks the state and timing info of scanned ports
pub struct PortStateTracker {
    states: DashMap<(IpAddr, u16), PortStateInfo>,
    sent_count: AtomicUsize,
    open_count: AtomicUsize,
    closed_count: AtomicUsize,
    filtered_count: AtomicUsize,
}

/// Information about a scanned port
#[derive(Debug, Clone)]
pub struct PortStateInfo {
    pub state: PortState,
    pub sent_at: Instant,
    pub retries: u32,
    pub response_time: Option<Duration>,
}

impl PortStateTracker {
    pub fn new() -> Self {
        Self {
            states: DashMap::new(),
            sent_count: AtomicUsize::new(0),
            open_count: AtomicUsize::new(0),
            closed_count: AtomicUsize::new(0),
            filtered_count: AtomicUsize::new(0),
        }
    }

    /// Registers a port that has just been pinged with SYN
    pub fn mark_sent(&self, ip: IpAddr, port: u16) {
        self.states.insert((ip, port), PortStateInfo {
            state: PortState::Unknown,
            sent_at: Instant::now(),
            retries: 0,
            response_time: None,
        });
        self.sent_count.fetch_add(1, Ordering::Relaxed);
    }

    /// Tracks a port's retry count
    pub fn increment_retry(&self, ip: IpAddr, port: u16) -> u32 {
        if let Some(mut info) = self.states.get_mut(&(ip, port)) {
            info.retries += 1;
            info.sent_at = Instant::now();
            return info.retries;
        }
        0
    }

    /// Logs an incoming SYN-ACK reply (port is open)
    pub fn mark_open(&self, ip: IpAddr, port: u16) {
        let now = Instant::now();
        if let Some(mut info) = self.states.get_mut(&(ip, port)) {
            let response_time = now.duration_since(info.sent_at);
            info.state = PortState::Open;
            info.response_time = Some(response_time);
        } else {
            // Unsolicited or late packet - still count it
            self.states.insert((ip, port), PortStateInfo {
                state: PortState::Open,
                sent_at: now,
                retries: 0,
                response_time: Some(Duration::from_secs(0)),
            });
        }
        self.open_count.fetch_add(1, Ordering::Relaxed);
    }

    /// Logs an incoming RST reply (port is closed)
    pub fn mark_closed(&self, ip: IpAddr, port: u16) {
        let now = Instant::now();
        if let Some(mut info) = self.states.get_mut(&(ip, port)) {
            let response_time = now.duration_since(info.sent_at);
            info.state = PortState::Closed;
            info.response_time = Some(response_time);
        } else {
            self.states.insert((ip, port), PortStateInfo {
                state: PortState::Closed,
                sent_at: now,
                retries: 0,
                response_time: Some(Duration::from_secs(0)),
            });
        }
        self.closed_count.fetch_add(1, Ordering::Relaxed);
    }

    /// Examines pending packets that timed out. Maps Unknown to Filtered.
    pub fn finalize_timeouts(&self, timeout_ms: u64) {
        let now = Instant::now();
        for mut entry in self.states.iter_mut() {
            if entry.value().state == PortState::Unknown {
                let elapsed = now.duration_since(entry.value().sent_at).as_millis() as u64;
                if elapsed > timeout_ms {
                    entry.value_mut().state = PortState::Filtered;
                    self.filtered_count.fetch_add(1, Ordering::Relaxed);
                }
            }
        }
    }

    /// Gets all results mapped as (IP, Port, State)
    pub fn get_results(&self) -> Vec<(IpAddr, u16, PortState)> {
        self.states.iter()
            .map(|entry| (entry.key().0, entry.key().1, entry.value().state))
            .collect()
    }

    /// Gets statistics about the scan
    pub fn get_stats(&self) -> ScanStats {
        ScanStats {
            total_sent: self.sent_count.load(Ordering::Relaxed),
            open_ports: self.open_count.load(Ordering::Relaxed),
            closed_ports: self.closed_count.load(Ordering::Relaxed),
            filtered_ports: self.filtered_count.load(Ordering::Relaxed),
        }
    }

    /// Gets response time for a specific port (if available)
    pub fn get_response_time(&self, ip: IpAddr, port: u16) -> Option<Duration> {
        self.states.get(&(ip, port))
            .and_then(|info| info.response_time)
    }
}

#[derive(Debug, Clone)]
pub struct ScanStats {
    pub total_sent: usize,
    pub open_ports: usize,
    pub closed_ports: usize,
    pub filtered_ports: usize,
}
