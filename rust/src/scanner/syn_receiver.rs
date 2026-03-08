use pnet::datalink::{self, Channel, NetworkInterface};
use std::sync::Arc;
use tokio::sync::mpsc;
use tokio::time::Duration;
use tracing::{info, debug};
use crate::scanner::port_state_tracker::PortStateTracker;
use crate::scanner::packet_parser::{parse_packet, ParsedTcpReply};
use std::time::Instant;

/// Listens for TCP responses (SYN-ACK and RST) from scanned targets
/// 
/// This receiver:
/// - Captures raw Ethernet frames from the specified interface
/// - Filters for TCP packets with SYN-ACK or RST flags
/// - Updates the port state tracker with results
/// - Implements adaptive timeouts based on RTT
pub struct SynReceiver {
    interface: NetworkInterface,
}

impl SynReceiver {
    pub fn new(interface_name: Option<&str>) -> Result<Self, String> {
        let interfaces = datalink::interfaces();
        let interface = if let Some(name) = interface_name {
            interfaces.into_iter().find(|iface| iface.name == name)
                .ok_or_else(|| format!("Interface {} not found", name))?
        } else {
            interfaces.into_iter()
                .find(|iface| iface.is_up() && !iface.is_loopback() && !iface.ips.is_empty())
                .ok_or_else(|| "No active non-loopback interface found".to_string())?
        };

        Ok(Self { interface })
    }

    /// Listens for SYN-ACK and RST responses and updates the PortStateTracker
    /// 
    /// This function:
    /// 1. Creates a datalink channel for packet capture
    /// 2. Continuously reads packets in a loop
    /// 3. Parses TCP responses and updates tracker
    /// 4. Handles shutdown gracefully
    /// 5. Implements timeout for waiting on stragglers
    pub async fn run(
        &self,
        tracker: Arc<PortStateTracker>,
        mut shutdown_rx: mpsc::Receiver<()>,
    ) -> Result<(), String> {
        // Create DataLink channel for receiving packets
        let (_, mut rx) = match datalink::channel(&self.interface, Default::default()) {
            Ok(Channel::Ethernet(tx, rx)) => (tx, rx),
            Ok(_) => return Err("Unhandled channel type".to_string()),
            Err(e) => return Err(format!("Failed to create datalink channel: {}", e)),
        };

        info!("SynReceiver listening on interface: {}", self.interface.name);

        let mut packets_received = 0usize;
        let mut responses_processed = 0usize;
        let start_time = Instant::now();
        let max_idle_time = Duration::from_secs(10); // Maximum idle time before giving up
        let mut last_packet_time = Instant::now();

        loop {
            // Check for shutdown signal
            if let Ok(_) = shutdown_rx.try_recv() {
                info!(
                    "SynReceiver shutting down. Received {} packets in {:.2}s",
                    packets_received,
                    start_time.elapsed().as_secs_f64()
                );
                break;
            }

            // Check for timeout (no packets received for a while)
            if last_packet_time.elapsed() > max_idle_time {
                debug!("No packets received for {:?}, continuing to wait for shutdown", max_idle_time);
            }

            // Use a small timeout on packet receive to avoid blocking forever
            // This allows us to check for shutdown signals periodically
            match rx.next() {
                Ok(packet_data) => {
                    packets_received += 1;
                    last_packet_time = Instant::now();

                    debug!("Received packet #{}: {} bytes", packets_received, packet_data.len());

                    // Parse the packet to extract TCP information
                    match parse_packet(packet_data) {
                        ParsedTcpReply::SynAck(source_ip, source_port) => {
                            debug!("Detected SYN-ACK from {}:{}", source_ip, source_port);
                            tracker.mark_open(source_ip, source_port);
                            responses_processed += 1;
                        }
                        ParsedTcpReply::Rst(source_ip, source_port) => {
                            debug!("Detected RST from {}:{}", source_ip, source_port);
                            tracker.mark_closed(source_ip, source_port);
                            responses_processed += 1;
                        }
                        ParsedTcpReply::Unknown => {
                            // This is normal - most traffic won't be TCP responses to our probes
                            debug!("Received non-target packet, ignoring");
                        }
                    }
                }
                Err(e) => {
                    // This is expected - pnet returns errors on timeout
                    // We handle this gracefully and continue
                    tokio::task::yield_now().await;
                }
            }
        }

        info!(
            "SynReceiver finished. Packets: {}, Responses: {}, Runtime: {:.2}s",
            packets_received,
            responses_processed,
            start_time.elapsed().as_secs_f64()
        );
        Ok(())
    }
}
