use pnet::packet::ethernet::{EtherTypes, EthernetPacket};
use pnet::packet::ipv4::Ipv4Packet;
use pnet::packet::tcp::TcpPacket;
use pnet::packet::Packet;
use std::net::IpAddr;

#[derive(Debug)]
pub enum ParsedTcpReply {
    SynAck(IpAddr, u16),
    Rst(IpAddr, u16),
    Unknown,
}

/// Parses raw Ethernet frames looking for TCP SYN-ACKs or RSTs from targets
pub fn parse_packet(packet_data: &[u8]) -> ParsedTcpReply {
    let ethernet = match EthernetPacket::new(packet_data) {
        Some(e) => e,
        None => return ParsedTcpReply::Unknown,
    };

    if ethernet.get_ethertype() != EtherTypes::Ipv4 {
        return ParsedTcpReply::Unknown;
    }

    let ipv4 = match Ipv4Packet::new(ethernet.payload()) {
        Some(ip) => ip,
        None => return ParsedTcpReply::Unknown, // Not IPv4
    };

    if ipv4.get_next_level_protocol() != pnet::packet::ip::IpNextHeaderProtocols::Tcp {
        return ParsedTcpReply::Unknown;
    }

    let tcp = match TcpPacket::new(ipv4.payload()) {
        Some(tcp) => tcp,
        None => return ParsedTcpReply::Unknown,
    };

    // A SYN-ACK has the SYN (0x02) and ACK (0x10) flags set = 0x12
    let flags = tcp.get_flags();
    if (flags & pnet::packet::tcp::TcpFlags::SYN) != 0 && (flags & pnet::packet::tcp::TcpFlags::ACK) != 0 {
        return ParsedTcpReply::SynAck(IpAddr::V4(ipv4.get_source()), tcp.get_source());
    }

    // A RST has the RST (0x04) flag set
    if (flags & pnet::packet::tcp::TcpFlags::RST) != 0 {
        return ParsedTcpReply::Rst(IpAddr::V4(ipv4.get_source()), tcp.get_source());
    }

    ParsedTcpReply::Unknown
}
