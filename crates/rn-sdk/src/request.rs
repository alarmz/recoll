use serde::{Deserialize, Serialize};

/// 搜尋請求
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchRequest {
    pub query: String,
    pub limit: usize,
    pub offset: usize,
}

impl SearchRequest {
    /// 便利建構
    pub fn new(query: impl Into<String>) -> Self {
        Self {
            query: query.into(),
            limit: 20,
            offset: 0,
        }
    }

    /// 驗證請求
    pub fn validate(&self) -> Result<(), String> {
        if self.query.is_empty() {
            return Err("empty query".to_string());
        }
        if self.limit == 0 {
            return Err("invalid limit".to_string());
        }
        Ok(())
    }
}
