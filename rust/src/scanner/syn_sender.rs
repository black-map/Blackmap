use pnet::datalink;
use pnet::datalink::{Channel, NetworkInterface};
use pnet::packet::ethernet::{EtherTypes, MutableEthernetPacket};
use pnet::packet::ipv4::MutableIpv4Packet;
use pnet::packet::tcp::{MutableTcpPacket, TcpPacket, TcpFlags};
use pnet::packet::MutablePacket;
use pnet::packet::ip::IpNextHeaderProtocols;
use std::net::{IpAddr, Ipv4Addr};
use std::sync::Arc;
use tokio::time::{sleep, Duration, Instant};
use tracing::{info, warn, debug};
use crate::scanner::target_scheduler::TargetScheduler;
use crate::scanner::port_state_tracker::PortStateTracker;

/// Builds and transmits raw TCP SYN packets using pnet's datalink layer.
pub struct SynSender {
    interface: NetworkInterface,
    source_ip: Ipv4Addr,
    source_mac: pnet::datalink::MacAddr,
}

impl SynSender {
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

        let source_ip = interface.ips.iter()
            .find(|ip| ip.is_ipv4())
            .map(|ip| match ip.ip() {
                IpAddr::V4(ipv4) => ipv4,
                _ => unreachable!(),
            })
            .ok_or_else(|| "Interface has no IPv4 address".to_string())?;

        let source_mac = interface.mac.ok_or_else(|| "Interface has no MAC address".to_string())?;

        info!(
            "SynSender initialized on {} (IP: {}, MAC: {})",
            interface.name, source_ip, source_mac
        );

        Ok(Self {
            interface,
            source_ip,
            source_mac,
        })
    }

    /// Generates a pseudorandom source port
    #[inline]
    fn random_source_port(&self, port: u16) -> u16 {
        let seed = rand::random::<u16>();
        49152 + ((seed ^ port) % (65535 - 49152))
    }

    /// Builds a complete TCP SYN packet (Ethernet + IPv4 + TCP)
    fn build_syn_packet(
        &self,
        buffer: &mut [u8],
        target_ip: Ipv4Addr,
        target_port: u16,
        dest_mac: pnet::datalink::MacAddr,
    ) -> Result<usize, String> {
        if buffer.len() < 54 {
            return Err("Buffer too small".to_string());
        }

        // Zero-initialize the buffer
        buffer[..54].iter_mut().for_each(|b| *b = 0);

        // Create Ethernet packet
        let mut eth_pkt = MutableEthernetPacket::new(&mut buffer[..54])
            .ok_or("Failed to create Ethernet packet")?;
        eth_pkt.set_destination(dest_mac);
        eth_pkt.set_source(self.source_mac);
        eth_pkt.set_ethertype(EtherTypes::Ipv4);

        // Create IPv4 packet from Ethernet payload
        let mut ipv4_pkt = MutableIpv4Packet::new(eth_pkt.payload_mut())
            .ok_or("Failed to create IPv4 packet")?;
        ipv4_pkt.set_version(4);
        ipv4_pkt.set_header_length(5);
        ipv4_pkt.set_total_length(40); // IPv4 header (20) + TCP header (20)
        ipv4_pkt.set_ttl(64);
        ipv4_pkt.set_next_level_protocol(IpNextHeaderProtocols::Tcp);
        ipv4_pkt.set_flags(0);
        ipv4_pkt.set_fragment_offset(0);
        ipv4_pkt.set_source(self.source_ip);
        ipv4_pkt.set_destination(target_ip);
        ipv4_pkt.set_checksum(0);

        // Create TCP packet from IPv4 payload
        let mut tcp_pkt = MutableTcpPacket::new(ipv4_pkt.payload_mut())
            .ok_or("Failed to create TCP packet")?;
        tcp_pkt.set_source(self.random_source_port(target_port));
        tcp_pkt.set_destination(target_port);
        tcp_pkt.set_sequence(rand::random::<u32>());
        tcp_pkt.set_acknowledgement(0);
        tcp_pkt.set_data_offset(5);
        tcp_pkt.set_flags(TcpFlags::SYN);
        tcp_pkt.set_window(64240);
        tcp_pkt.set_checksum(0);

        // Calculate TCP checksum
        let tcp_checksum = pnet::packet::tcp::ipv4_checksum(
            &tcp_pkt.to_immutable(),
            &self.source_ip,
            &target_ip,
        );
        tcp_pkt.set_checksum(tcp_checksum);

        // Calculate IPv4 checksum
        let ipv4_checksum = pnet::packet::ipv4::checksum(&ipv4_pkt.to_immutable());
        ipv4_pkt.set_checksum(ipv4_checksum);

        Ok(54)
    }

    /// Sends SYN packets to all scheduled targets
    pub async fn run(
        &self,
        scheduler: Arc<TargetScheduler>,
        tracker: Arc<PortStateTracker>,
        rate_limit: u32,
    ) -> Result<(), String> {
        // Open datalink channel for transmission
        let (mut tx, _) = match datalink::channel(&self.interface, Default::default()) {
            Ok(Channel::Ethernet(tx, rx)) => (tx, rx),
            Ok(_) => return Err("Unsupported channel type".to_string()),
            Err(e) => return Err(format!("Failed to open channel: {}", e)),
        };

        // Use broadcast MAC for routing
        let dest_mac = pnet::datalink::MacAddr::broadcast();

        let batch_size = std::cmp::max(10, (rate_limit / 100).max(100)) as usize;
        let mut packet_count = 0u32;
        let mut window_start = Instant::now();

        info!("SynSender starting transmission");

        let mut buffer = [0u8; 100];

        while !scheduler.is_depleted() {
            let batch = scheduler.next_batch(batch_size);

            if batch.is_empty() {
                break;
            }

            for (target_ip, port) in batch {
                let IpAddr::V4(target_ipv4) = target_ip else {
                    continue;
                };

                // Build packet
                match self.build_syn_packet(&mut buffer, target_ipv4, port, dest_mac) {
                    Ok(size) => {
                        // Send packet
                        let result = tx.send_to(&buffer[..size], None);
                        match result {
                            Some(Ok(_)) => {
                                debug!("Sent SYN to {}:{}", target_ipv4, port);
                                tracker.mark_sent(target_ip, port);
                                packet_count += 1;
                            }
                            Some(Err(e)) => {
                                warn!("Failed to send SYN to {}:{}: {}", target_ipv4, port, e);
                            }
                            None => {
                                warn!("TX buffer full for {}:{}", target_ipv4, port);
                            }
                        }
                    }
                    Err(e) => {
                        warn!("Failed to build packet for {}:{}: {}", target_ipv4, port, e);
                    }
                }
            }

            // Rate limiting
            if rate_limit > 0 {
                let elapsed = window_start.elapsed();
                let target_window = Duration::from_millis(100);
                let packets_per_100ms = (rate_limit as u64 / 10) as u32;

                if packet_count >= packets_per_100ms && elapsed < target_window {
                    sleep(target_window - elapsed).await;
                    packet_count = 0;
                    window_start = Instant::now();
                }
            }

            tokio::task::yield_now().await;
        }

        info!("SynSender completed transmission");
        Ok(())
    }
}
