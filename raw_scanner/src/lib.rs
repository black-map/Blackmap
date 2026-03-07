//! High-performance Stateless TCP SYN Scanner using Raw Sockets (Masscan-style)
//!
//! Features lockdown-free memory architectures for millions of PPS (Packets per second)
//! tracking SYN-ACKs independently via background kernel epoll/bpf filters.

pub mod sender;
pub mod receiver;
pub mod engine;

pub use engine::StatelessScanner;
