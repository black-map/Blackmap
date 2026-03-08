use pnet::packet::ethernet::{EtherTypes, EthernetPacket};
use pnet::packet::ipv4::Ipv4Packet;
use pnet::packet::ipv6::Ipv6Packet;
use pnet::packet::tcp::TcpPacket;
use pnet::packet::Packet;
use std::net::IpAddr;
use tracing::debug;

/// Represents a parsed TCP response from a target
#[derive(Debug, Clone)]
pub enum ParsedTcpReply {
    /// SYN-ACK received: port is open
    SynAck(IpAddr, u16),
    /// RST received: port is closed
    Rst(IpAddr, u16),
    /// Unknown response or not TCP
    Unknown,
}

/// Parses raw Ethernet frames looking for TCP SYN-ACKs or RSTs from targets
/// 
/// This function processes raw packet data and extracts:
/// - Source IP address (IPv4 or IPv6)
/// - Source port (TCP source port from the response)
/// - Response type (SYN-ACK or RST)
pub fn parse_packet(packet_data: &[u8]) -> ParsedTcpReply {
    // Parse Ethernet layer
    let ethernet = match EthernetPacket::new(packet_data) {
        Some(e) => e,
        None => return ParsedTcpReply::Unknown,
    };

    // We're interested in IPv4 or IPv6 packets
    match ethernet.get_ethertype() {
        EtherTypes::Ipv4 => parse_ipv4_packet(ethernet.payload()),
        EtherTypes::Ipv6 => parse_ipv6_packet(ethernet.payload()),
        _ => ParsedTcpReply::Unknown,
    }
}

/// Parses IPv4 packets for TCP responses
fn parse_ipv4_packet(payload: &[u8]) -> ParsedTcpReply {
    let ipv4 = match Ipv4Packet::new(payload) {
        Some(ip) => ip,
        None => return ParsedTcpReply::Unknown,
    };

    // We only care about TCP
    if ipv4.get_next_level_protocol() != pnet::packet::ip::IpNextHeaderProtocols::Tcp {
        return ParsedTcpReply::Unknown;
    }

    parse_tcp_packet(IpAddr::V4(ipv4.get_source()), ipv4.payload())
}

/// Parses IPv6 packets for TCP responses
fn parse_ipv6_packet(payload: &[u8]) -> ParsedTcpReply {
    let ipv6 = match Ipv6Packet::new(payload) {
        Some(ip) => ip,
        None => return ParsedTcpReply::Unknown,
    };

    // We only care about TCP (next header = 6)
    if ipv6.get_next_header() != pnet::packet::ip::IpNextHeaderProtocols::Tcp {
        return ParsedTcpReply::Unknown;
    }

    parse_tcp_packet(IpAddr::V6(ipv6.get_source()), ipv6.payload())
}

/// Parses TCP packet and classifies as SYN-ACK or RST
fn parse_tcp_packet(source_ip: IpAddr, payload: &[u8]) -> ParsedTcpReply {
    let tcp = match TcpPacket::new(payload) {
        Some(t) => t,
        None => return ParsedTcpReply::Unknown,
    };

    let flags = tcp.get_flags();
    let source_port = tcp.get_source();
    let dest_port = tcp.get_destination();

    // TCP flags breakdown: bit order is FIN|SYN|RST|PSH|ACK|URG|ECE|CWR
    // FIN = 0x01
    // SYN = 0x02
    // RST = 0x04
    // PSH = 0x08
    // ACK = 0x10
    // URG = 0x20
    // ECE = 0x40
    // CWR = 0x80
    
    let syn_flag = (flags & 0x02) != 0;
    let ack_flag = (flags & 0x10) != 0;
    let rst_flag = (flags & 0x04) != 0;

    debug!(
        "TCP from {}:{} to port {} - flags: 0x{:02x} (SYN={}, ACK={}, RST={})",
        source_ip, source_port, dest_port, flags, syn_flag, ack_flag, rst_flag
    );

    // Response to our SYN probe:
    // - SYN-ACK: Port is OPEN
    // - RST: Port is CLOSED
    // - Other responses: Port might be filtered or host is sending something unexpected
    
    if syn_flag && ack_flag {
        debug!("Classified as OPEN (SYN-ACK): {}:{}", source_ip, source_port);
        ParsedTcpReply::SynAck(source_ip, source_port)
    } else if rst_flag {
        debug!("Classified as CLOSED (RST): {}:{}", source_ip, source_port);
        ParsedTcpReply::Rst(source_ip, source_port)
    } else {
        // Other TCP flags (e.g., SYN alone, ACK alone, FIN, etc.)
        // These shouldn't happen in response to our SYN probes
        debug!(
            "Unexpected TCP flags from {}:{}: 0x{:02x}",
            source_ip, source_port, flags
        );
        ParsedTcpReply::Unknown
    }
}
