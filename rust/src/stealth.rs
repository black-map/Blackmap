//! Stealth and evasion engine for BlackMap
//!
//! Implements multiple stealth techniques:
//! - Packet fragmentation
//! - TCP option randomization
//! - TTL manipulation
//! - Packet padding
//! - Decoy scanning
//! - Jitter injection
//! - Adaptive timing

use serde::{Deserialize, Serialize};
use std::time::Duration;
use rand::Rng;

/// Stealth level (0-5)
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq, PartialOrd, Ord)]
pub enum StealthLevel {
    /// Performance mode - no stealth
    Performance = 0,

    /// Normal mode - minimal stealth
    Normal = 1,

    /// Quiet mode - moderate stealth
    Quiet = 2,

    /// Stealth mode - significant stealth measures
    Stealth = 3,

    /// Ultra stealth - extreme evasion
    UltraStealth = 4,

    /// Paranoid mode - maximum stealth (slowest)
    Paranoid = 5,
}

/// Stealth configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StealthConfig {
    /// Stealth level
    pub level: StealthLevel,

    /// Enable packet fragmentation
    pub fragmentation: bool,

    /// Enable TCP option randomization
    pub randomize_tcp_options: bool,

    /// Enable TTL manipulation
    pub ttl_variation: bool,

    /// Enable packet padding
    pub add_padding: bool,

    /// Enable decoy scanning
    pub use_decoys: bool,

    /// Number of decoy IPs
    pub num_decoys: u8,

    /// Enable jitter
    pub add_jitter: bool,

    /// Jitter percentage (0-100)
    pub jitter_percent: u32,

    /// Minimum delay between probes (ms)
    pub min_delay_ms: u64,

    /// Maximum delay between probes (ms)
    pub max_delay_ms: u64,

    /// Randomize port order
    pub randomize_ports: bool,
}

impl StealthConfig {
    /// Create config from stealth level
    pub fn from_level(level: StealthLevel) -> Self {
        match level {
            StealthLevel::Performance => Self {
                level,
                fragmentation: false,
                randomize_tcp_options: false,
                ttl_variation: false,
                add_padding: false,
                use_decoys: false,
                num_decoys: 0,
                add_jitter: false,
                jitter_percent: 0,
                min_delay_ms: 0,
                max_delay_ms: 0,
                randomize_ports: false,
            },
            StealthLevel::Normal => Self {
                level,
                fragmentation: false,
                randomize_tcp_options: true,
                ttl_variation: false,
                add_padding: false,
                use_decoys: false,
                num_decoys: 0,
                add_jitter: true,
                jitter_percent: 10,
                min_delay_ms: 1,
                max_delay_ms: 10,
                randomize_ports: true,
            },
            StealthLevel::Quiet => Self {
                level,
                fragmentation: true,
                randomize_tcp_options: true,
                ttl_variation: true,
                add_padding: true,
                use_decoys: false,
                num_decoys: 0,
                add_jitter: true,
                jitter_percent: 25,
                min_delay_ms: 10,
                max_delay_ms: 100,
                randomize_ports: true,
            },
            StealthLevel::Stealth => Self {
                level,
                fragmentation: true,
                randomize_tcp_options: true,
                ttl_variation: true,
                add_padding: true,
                use_decoys: true,
                num_decoys: 2,
                add_jitter: true,
                jitter_percent: 50,
                min_delay_ms: 100,
                max_delay_ms: 500,
                randomize_ports: true,
            },
            StealthLevel::UltraStealth => Self {
                level,
                fragmentation: true,
                randomize_tcp_options: true,
                ttl_variation: true,
                add_padding: true,
                use_decoys: true,
                num_decoys: 5,
                add_jitter: true,
                jitter_percent: 75,
                min_delay_ms: 500,
                max_delay_ms: 2000,
                randomize_ports: true,
            },
            StealthLevel::Paranoid => Self {
                level,
                fragmentation: true,
                randomize_tcp_options: true,
                ttl_variation: true,
                add_padding: true,
                use_decoys: true,
                num_decoys: 10,
                add_jitter: true,
                jitter_percent: 100,
                min_delay_ms: 2000,
                max_delay_ms: 5000,
                randomize_ports: true,
            },
        }
    }
}

/// Stealth engine
pub struct StealthEngine {
    config: StealthConfig,
}

impl StealthEngine {
    /// Create a new stealth engine
    pub fn new(config: StealthConfig) -> Self {
        Self { config }
    }

    /// Get next delay based on stealth settings
    pub fn get_delay(&self) -> Duration {
        let mut rng = rand::thread_rng();
        let base_delay = rng.gen_range(self.config.min_delay_ms..=self.config.max_delay_ms);
        let jitter = if self.config.add_jitter {
            let jitter_amount = (base_delay as f64 * self.config.jitter_percent as f64) / 100.0;
            rng.gen_range(0..=jitter_amount as u64)
        } else {
            0
        };
        Duration::from_millis(base_delay + jitter)
    }

    /// Check if should apply fragmentation
    pub fn should_fragment(&self) -> bool {
        self.config.fragmentation
    }

    /// Check if should randomize TCP options
    pub fn should_randomize_tcp(&self) -> bool {
        self.config.randomize_tcp_options
    }

    /// Check if should use decoys
    pub fn should_use_decoys(&self) -> bool {
        self.config.use_decoys
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_stealth_config_levels() {
        let perf = StealthConfig::from_level(StealthLevel::Performance);
        assert!(!perf.add_jitter);

        let paranoid = StealthConfig::from_level(StealthLevel::Paranoid);
        assert!(paranoid.add_jitter);
        assert!(paranoid.use_decoys);
    }

    #[test]
    fn test_stealth_delays() {
        let config = StealthConfig::from_level(StealthLevel::Normal);
        let engine = StealthEngine::new(config);

        let delay = engine.get_delay();
        assert!(delay.as_millis() >= 1);
        assert!(delay.as_millis() <= 10);
    }
}
