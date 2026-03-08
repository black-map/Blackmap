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
        // MongoDB handshake - isMaster command
        // MongoDB 3.6+ uses OP_MSG, but for simplicity, try older OP_QUERY
        let ismaster_query = [
            0x41, 0x00, 0x00, 0x00, // message length (65)
            0x00, 0x00, 0x00, 0x00, // request id
            0x00, 0x00, 0x00, 0x00, // response to
            0xd4, 0x07, 0x00, 0x00, // opCode (2004 = OP_QUERY)
            0x00, 0x00, 0x00, 0x00, // flags
            0x61, 0x64, 0x6d, 0x69, 0x6e, 0x2e, 0x24, 0x63, 0x6d, 0x64, 0x00, // "admin.$cmd"
            0x00, 0x00, 0x00, 0x00, // number to skip
            0x01, 0x00, 0x00, 0x00, // number to return
            0x1b, 0x00, 0x00, 0x00, // document length
            0x10, 0x69, 0x73, 0x4d, 0x61, 0x73, 0x74, 0x65, 0x72, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 // isMaster: true
        ];
        
        if tokio::time::timeout(Duration::from_secs(5), stream.write_all(&ismaster_query)).await.is_err() {
            return None;
        }
        
        let mut buffer = [0u8; 512];
        match tokio::time::timeout(Duration::from_secs(5), stream.read(&mut buffer)).await {
            Ok(Ok(n)) if n > 16 => {
                // MongoDB response starts with message length
                // Check if response looks like MongoDB
                if buffer[12] == 0x01 && buffer[13] == 0x00 { // response flags ok
                    // Try to extract version from response document
                    // This is complex, but for now, if we get a valid response, assume MongoDB
                    return Some(ServiceInfo {
                        service: "mongodb".to_string(),
                        version: None, // Would need more parsing for version
                        confidence: 85,
                    });
                }
            }
            _ => {}
        }
        
        None
    }
}