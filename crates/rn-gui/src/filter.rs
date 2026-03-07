use serde::{Deserialize, Serialize};

/// 篩選狀態
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct FilterState {
    pub file_type: Option<String>,
    pub path_prefix: Option<String>,
    pub date_from: Option<String>,
}

impl FilterState {
    /// 是否有任何篩選條件啟用
    pub fn is_active(&self) -> bool {
        self.file_type.is_some() || self.path_prefix.is_some() || self.date_from.is_some()
    }

    /// 重設所有篩選條件
    pub fn clear(&mut self) {
        self.file_type = None;
        self.path_prefix = None;
        self.date_from = None;
    }
}
