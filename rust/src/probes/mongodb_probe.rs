use super::{ServiceProbe, ServiceInfo};
use tokio::net::TcpStream;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::time::Duration;

pub struct MongodbProbe;

impl ServiceProbe for MongodbProbe {
    fn name(&self) -> &'static str {
        "mongodb"
    }

    fn ports(&self) -> Vec<u16> {
        vec![27017]
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        // Send a MongoDB buildInfo command using OP_QUERY
        let buildinfo_query = [
            0x3a, 0x00, 0x00, 0x00, // message length (58)
            0x01, 0x00, 0x00, 0x00, // request id
            0x00, 0x00, 0x00, 0x00, // response to
            0xd4, 0x07, 0x00, 0x00, // opCode (2004 = OP_QUERY)
            0x00, 0x00, 0x00, 0x00, // flags
            0x61, 0x64, 0x6d, 0x69, 0x6e, 0x2e, 0x24, 0x63, 0x6d, 0x64, 0x00, // "admin.$cmd"
            0x00, 0x00, 0x00, 0x00, // number to skip
            0x01, 0x00, 0x00, 0x00, // number to return
            // BSON document for { buildinfo: 1 }
            0x14, 0x00, 0x00, 0x00, // document length (20)
            0x10, // type: 32-bit int
            0x62, 0x75, 0x69, 0x6c, 0x64, 0x69, 0x6e, 0x66, 0x6f, 0x00, // "buildinfo"
            0x01, 0x00, 0x00, 0x00, // 1
            0x00 // document terminator
        ];
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(&buildinfo_query)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 1024];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 16 => {
                parse_mongodb_response(&buffer[..n])
            }
            _ => None,
        }
    }
}

/// Parses the MongoDB OP_REPLY BSON response. Extracted version if it was a buildInfo command.
pub fn parse_mongodb_response(buffer: &[u8]) -> Option<ServiceInfo> {
    if buffer.len() < 16 {
        return None;
    }

    // Read message length and validate
    let msg_length = u32::from_le_bytes([buffer[0], buffer[1], buffer[2], buffer[3]]) as usize;
    if msg_length > buffer.len() || msg_length < 16 {
        return None;
    }

    // Read OpCode (bytes 12..16)
    let op_code = u32::from_le_bytes([buffer[12], buffer[13], buffer[14], buffer[15]]);
    
    // 1 = OP_REPLY
    // 2013 = OP_MSG (Modern MongoDB responses)
    if op_code != 1 && op_code != 2013 {
        return None;
    }

    // Try to extract "version" from the BSON document manually.
    // BSON strings are prefixed by length and null-terminated.
    // We can do a simple byte search for "version\x00" or similar keys returned by buildInfo.
    
    let haystack = &buffer[16..msg_length];
    
    // Look for BSON string type (0x02) followed by "version\x00"
    let key = b"\x02version\x00";
    if let Some(pos) = haystack.windows(key.len()).position(|w| w == key) {
        let val_start = pos + key.len();
        if val_start + 4 <= haystack.len() {
            // Read string length (int32)
            let str_len = u32::from_le_bytes([
                haystack[val_start],
                haystack[val_start+1],
                haystack[val_start+2],
                haystack[val_start+3]
            ]) as usize;
            
            let str_data_start = val_start + 4;
            if str_data_start + str_len <= haystack.len() && str_len > 0 {
                // Read up to str_len - 1 (excluding null terminator)
                let version_str = String::from_utf8_lossy(&haystack[str_data_start..str_data_start + str_len - 1]);
                
                return Some(ServiceInfo {
                    service: "mongodb".to_string(),
                    version: Some(format!("MongoDB {}", version_str)),
                    confidence: 95,
                });
            }
        }
    }

    // If we couldn't parse the version but it replied with a valid opcode
    Some(ServiceInfo {
        service: "mongodb".to_string(),
        version: None,
        confidence: 85,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_mongodb_buildinfo() {
        // Hex dump of a MongoDB 5.0.14 OP_REPLY to buildInfo
        let mut packet = vec![
            0x8c, 0x00, 0x00, 0x00, // msg length
            0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00, // responseTo
            0x01, 0x00, 0x00, 0x00, // OP_REPLY
            //... skipping some OP_REPLY headers
            0x00, 0x00, 0x00, 0x00, // flags
            0x00, 0x00, 0x00, 0x00, // cursorID (8 bytes)
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, // startingFrom
            0x01, 0x00, 0x00, 0x00, // numReturn
        ];
        
        // BSON document snippet
        let mut document = vec![
            0x00, 0x00, 0x00, 0x00, // doc length placeholder
            0x02, b'v', b'e', b'r', b's', b'i', b'o', b'n', 0x00,
            0x07, 0x00, 0x00, 0x00, // string length (7)
            b'5', b'.', b'0', b'.', b'1', b'4', 0x00,
            0x00 // doc terminator
        ];
        
        packet.append(&mut document);
        
        // Fix up packet length
        let len = packet.len() as u32;
        packet[0] = (len & 0xff) as u8;
        packet[1] = ((len >> 8) & 0xff) as u8;

        let info = parse_mongodb_response(&packet).unwrap();
        assert_eq!(info.service, "mongodb");
        assert_eq!(info.version.unwrap(), "MongoDB 5.0.14");
    }

    #[test]
    fn test_parse_invalid() {
        let response = b"HTTP/1.1 200 OK\r\n\r\n";
        assert!(parse_mongodb_response(response).is_none());
    }
}