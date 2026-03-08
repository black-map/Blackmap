use std::net::IpAddr;
use dashmap::DashMap;
use crate::scanner::PortState;
use std::time::Instant;

/// Tracks the state and timing info of scanned ports
pub struct PortStateTracker {
    states: DashMap<(IpAddr, u16), PortStateInfo>,
}

#[derive(Debug, Clone)]
pub struct PortStateInfo {
    pub state: PortState,
    pub sent_at: Instant,
    pub retries: u32,
}

impl PortStateTracker {
    pub fn new() -> Self {
        Self {
            states: DashMap::new(),
        }
    }

    /// Registers a port that has just been pinged with SYN
    pub fn mark_sent(&self, ip: IpAddr, port: u16) {
        self.states.insert((ip, port), PortStateInfo {
            state: PortState::Unknown,
            sent_at: Instant::now(),
            retries: 0,
        });
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

    /// Logs an incoming SYN-ACK reply
    pub fn mark_open(&self, ip: IpAddr, port: u16) {
        if let Some(mut info) = self.states.get_mut(&(ip, port)) {
            info.state = PortState::Open;
        } else {
            // Unsolicited or late packet
            self.states.insert((ip, port), PortStateInfo {
                state: PortState::Open,
                sent_at: Instant::now(),
                retries: 0,
            });
        }
    }

    /// Logs an incoming RST reply
    pub fn mark_closed(&self, ip: IpAddr, port: u16) {
        if let Some(mut info) = self.states.get_mut(&(ip, port)) {
            info.state = PortState::Closed;
        }
    }

    /// Examines pending packets that timed out based on RTT. Map unknown to filtered.
    pub fn finalize_timeouts(&self, timeout_ms: u64) {
        let now = Instant::now();
        for mut entry in self.states.iter_mut() {
            if entry.value().state == PortState::Unknown {
                if now.duration_since(entry.value().sent_at).as_millis() as u64 > timeout_ms {
                    entry.value_mut().state = PortState::Filtered;
                }
            }
        }
    }

    /// Gets all results mapped
    pub fn get_results(&self) -> Vec<(IpAddr, u16, PortState)> {
        self.states.iter()
            .map(|entry| (entry.key().0, entry.key().1, entry.value().state))
            .collect()
    }
}
