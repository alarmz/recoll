use serde::{Deserialize, Serialize};

/// 應用程式狀態
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum AppState {
    Idle,
    Indexing,
    Paused,
}

/// 狀態列資訊
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct StatusInfo {
    pub state: AppState,
    pub indexed_count: usize,
    pub queue_size: usize,
    pub progress: u8,
}

impl Default for StatusInfo {
    fn default() -> Self {
        Self {
            state: AppState::Idle,
            indexed_count: 0,
            queue_size: 0,
            progress: 0,
        }
    }
}
