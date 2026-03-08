use pnet::datalink::{self, Channel, NetworkInterface};
use std::sync::Arc;
use tokio::sync::mpsc;
use tracing::{info, warn};
use crate::scanner::port_state_tracker::PortStateTracker;
use crate::scanner::packet_parser::{parse_packet, ParsedTcpReply};

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

    /// Listens for SYN-ACK and RST responses and updates the PortStateTracker.
    pub async fn run(
        &self,
        tracker: Arc<PortStateTracker>,
        mut shutdown_rx: mpsc::Receiver<()>,
    ) -> Result<(), String> {
        // Create DataLink channel
        let (_, mut rx) = match datalink::channel(&self.interface, Default::default()) {
            Ok(Channel::Ethernet(tx, rx)) => (tx, rx),
            Ok(_) => return Err("Unhandled channel type".into()),
            Err(e) => return Err(format!("Failed to create datalink channel: {}", e)),
        };

        info!("SynReceiver armed. Listening on {}", self.interface.name);

        loop {
            // Check for shutdown signal
            if let Ok(_) = shutdown_rx.try_recv() {
                info!("SynReceiver shutting down.");
                break;
            }

            match rx.next() {
                Ok(packet) => {
                    match parse_packet(packet) {
                        ParsedTcpReply::SynAck(ip, port) => {
                            tracker.mark_open(ip, port);
                        }
                        ParsedTcpReply::Rst(ip, port) => {
                            tracker.mark_closed(ip, port);
                        }
                        ParsedTcpReply::Unknown => {
                            // Ignore unrelated traffic
                        }
                    }
                }
                Err(e) => {
                    // Timeout or error reading packet
                    // This is expected constantly in non-blocking reads if we use pnet with timeouts
                    tokio::task::yield_now().await;
                }
            }
        }

        Ok(())
    }
}
