/// 服務狀態
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ServiceStatus {
    Running,
    Stopped,
    Paused,
    Unknown,
}

impl ServiceStatus {
    /// 從字串解析服務狀態
    pub fn parse(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "running" => Self::Running,
            "stopped" => Self::Stopped,
            "paused" => Self::Paused,
            _ => Self::Unknown,
        }
    }
}
