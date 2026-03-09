pub mod ssh_probe;
pub mod http_probe;
pub mod mysql_probe;
pub mod postgres_probe;
pub mod redis_probe;
pub mod docker_probe;
pub mod mongodb_probe;
pub mod ftp_probe;
pub mod smtp_probe;
pub mod imap_pop3_probe;

use tokio::net::TcpStream;

#[derive(Debug, Clone)]
pub struct ServiceInfo {
    pub service: String,
    pub version: Option<String>,
    pub confidence: u8, // 0-100
}

pub trait ServiceProbe {
    fn name(&self) -> &'static str;
    fn ports(&self) -> Vec<u16>;
    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo>;
}

// Enum dispatcher to avoid object safety issues with async fn in traits
pub enum ProbeDispatcher {
    Ssh(ssh_probe::SshProbe),
    Http(http_probe::HttpProbe),
    Mysql(mysql_probe::MysqlProbe),
    Postgres(postgres_probe::PostgresProbe),
    Redis(redis_probe::RedisProbe),
    Docker(docker_probe::DockerProbe),
    Mongodb(mongodb_probe::MongodbProbe),
    Ftp(ftp_probe::FtpProbe),
    Smtp(smtp_probe::SmtpProbe),
    Imap(imap_pop3_probe::ImapProbe),
    Pop3(imap_pop3_probe::Pop3Probe),
}

impl ServiceProbe for ProbeDispatcher {
    fn name(&self) -> &'static str {
        match self {
            Self::Ssh(p) => p.name(),
            Self::Http(p) => p.name(),
            Self::Mysql(p) => p.name(),
            Self::Postgres(p) => p.name(),
            Self::Redis(p) => p.name(),
            Self::Docker(p) => p.name(),
            Self::Mongodb(p) => p.name(),
            Self::Ftp(p) => p.name(),
            Self::Smtp(p) => p.name(),
            Self::Imap(p) => p.name(),
            Self::Pop3(p) => p.name(),
        }
    }

    fn ports(&self) -> Vec<u16> {
        match self {
            Self::Ssh(p) => p.ports(),
            Self::Http(p) => p.ports(),
            Self::Mysql(p) => p.ports(),
            Self::Postgres(p) => p.ports(),
            Self::Redis(p) => p.ports(),
            Self::Docker(p) => p.ports(),
            Self::Mongodb(p) => p.ports(),
            Self::Ftp(p) => p.ports(),
            Self::Smtp(p) => p.ports(),
            Self::Imap(p) => p.ports(),
            Self::Pop3(p) => p.ports(),
        }
    }

    async fn probe(&self, stream: &mut TcpStream) -> Option<ServiceInfo> {
        match self {
            Self::Ssh(p) => p.probe(stream).await,
            Self::Http(p) => p.probe(stream).await,
            Self::Mysql(p) => p.probe(stream).await,
            Self::Postgres(p) => p.probe(stream).await,
            Self::Redis(p) => p.probe(stream).await,
            Self::Docker(p) => p.probe(stream).await,
            Self::Mongodb(p) => p.probe(stream).await,
            Self::Ftp(p) => p.probe(stream).await,
            Self::Smtp(p) => p.probe(stream).await,
            Self::Imap(p) => p.probe(stream).await,
            Self::Pop3(p) => p.probe(stream).await,
        }
    }
}

pub fn get_all_probes() -> Vec<ProbeDispatcher> {
    vec![
        ProbeDispatcher::Http(http_probe::HttpProbe),
        ProbeDispatcher::Ssh(ssh_probe::SshProbe),
        ProbeDispatcher::Ftp(ftp_probe::FtpProbe),
        ProbeDispatcher::Smtp(smtp_probe::SmtpProbe),
        ProbeDispatcher::Imap(imap_pop3_probe::ImapProbe),
        ProbeDispatcher::Pop3(imap_pop3_probe::Pop3Probe),
        ProbeDispatcher::Mysql(mysql_probe::MysqlProbe),
        ProbeDispatcher::Postgres(postgres_probe::PostgresProbe),
        ProbeDispatcher::Redis(redis_probe::RedisProbe),
        ProbeDispatcher::Docker(docker_probe::DockerProbe),
        ProbeDispatcher::Mongodb(mongodb_probe::MongodbProbe),
    ]
}

pub async fn detect_service(port: u16, stream: &mut TcpStream) -> Option<ServiceInfo> {
    for probe in get_all_probes() {
        if probe.ports().contains(&port) {
            if let Some(info) = probe.probe(stream).await {
                return Some(info);
            }
        }
    }
    None
}