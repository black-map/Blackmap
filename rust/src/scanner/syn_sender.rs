use pnet::datalink::{self, Channel, NetworkInterface};
use pnet::packet::ethernet::{EtherTypes, MutableEthernetPacket};
use pnet::packet::ipv4::MutableIpv4Packet;
use pnet::packet::tcp::{MutableTcpPacket, TcpFlags};
use pnet::packet::{Packet, MutablePacket};
use pnet::packet::ip::IpNextHeaderProtocols;
use std::net::{IpAddr, Ipv4Addr};
use std::sync::Arc;
use tokio::time::{sleep, Duration};
use tracing::{info, warn, error};
use crate::scanner::target_scheduler::TargetScheduler;
use crate::scanner::port_state_tracker::PortStateTracker;

/// Builds and transmits raw TCP SYN packets.
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

        Ok(Self {
            interface,
            source_ip,
            source_mac,
        })
    }

    /// Blasts SYN packets based on the scheduler
    pub async fn run(
        &self,
        scheduler: Arc<TargetScheduler>,
        tracker: Arc<PortStateTracker>,
        rate_limit: u32,
    ) -> Result<(), String> {
        // Create DataLink channel
        let (mut tx, _) = match datalink::channel(&self.interface, Default::default()) {
            Ok(Channel::Ethernet(tx, rx)) => (tx, rx),
            Ok(_) => return Err("Unhandled channel type".into()),
            Err(e) => return Err(format!("Failed to create datalink channel: {}", e)),
        };

        // Gateway MAC (Dummy for now, normally requires ARP resolution)
        // For local subnets we need ARP. For internet routing, we route to gateway MAC.
        // As a highly simplified abstraction for mass-scanning, we'll try to let standard routing handle it 
        // by sending standard IP packets if possible, but datalink requires MACs. 
        // A full implementation requires discovering the router's MAC via ARP.
        // *Mocked gateway MAC for compilation - in real life replace with ARP cache lookup*
        let dest_mac = pnet::datalink::MacAddr::broadcast(); 

        let batch_size = std::cmp::max(1, rate_limit / 10) as usize;
        let mut packets_sent_this_second = 0;
        let mut last_tick = std::time::Instant::now();

        info!("SynSender armed. Source IP: {}", self.source_ip);

        while !scheduler.is_depleted() {
            let batch = scheduler.next_batch(batch_size);
            
            for (target_ip, port) in batch {
                let IpAddr::V4(target_ipv4) = target_ip else {
                    continue; // Skip IPv6 for now in this MVP
                };

                // Build Packet Pipeline
                let mut buffer = [0u8; 54]; // Eth (14) + IPv4 (20) + TCP (20)
                
                let mut eth = MutableEthernetPacket::new(&mut buffer).unwrap();
                eth.set_destination(dest_mac);
                eth.set_source(self.source_mac);
                eth.set_ethertype(EtherTypes::Ipv4);
                
                let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
                ipv4.set_version(4);
                ipv4.set_header_length(5);
                ipv4.set_total_length(40); // 20 IP + 20 TCP
                ipv4.set_ttl(64);
                ipv4.set_next_level_protocol(IpNextHeaderProtocols::Tcp);
                ipv4.set_source(self.source_ip);
                ipv4.set_destination(target_ipv4);
                let cs = pnet::packet::ipv4::checksum(&ipv4.to_immutable());
                ipv4.set_checksum(cs);
                
                let mut tcp = MutableTcpPacket::new(ipv4.payload_mut()).unwrap();
                tcp.set_source(49152 + (port % 10000)); // ephemeral
                tcp.set_destination(port);
                tcp.set_sequence(rand::random::<u32>());
                tcp.set_acknowledgement(0);
                tcp.set_data_offset(5);
                tcp.set_flags(TcpFlags::SYN);
                tcp.set_window(64240);
                let chk = pnet::packet::tcp::ipv4_checksum(&tcp.to_immutable(), &self.source_ip, &target_ipv4);
                tcp.set_checksum(chk);
                
                // Transmit
                match tx.send_to(eth.packet(), None) {
                    Some(Ok(_)) => {
                        tracker.mark_sent(target_ip, port);
                        packets_sent_this_second += 1;
                    }
                    Some(Err(e)) => warn!("Failed to send packet to {}:{}: {}", target_ip, port, e),
                    None => warn!("TX buffer full"),
                }

                // Rate limiting
                if rate_limit > 0 && packets_sent_this_second >= rate_limit {
                    let elapsed = last_tick.elapsed();
                    if elapsed < Duration::from_secs(1) {
                        sleep(Duration::from_secs(1) - elapsed).await;
                    }
                    packets_sent_this_second = 0;
                    last_tick = std::time::Instant::now();
                }
            }
            
            // tiny yield to allow receiver to process
            tokio::task::yield_now().await;
        }

        info!("SynSender finished transmitting.");
        Ok(())
    }
}
