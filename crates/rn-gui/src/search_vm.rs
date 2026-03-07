use serde::{Deserialize, Serialize};

/// 搜尋結果項目
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SearchHit {
    pub path: String,
    pub snippet: String,
    pub score: f32,
}

/// 搜尋視圖模型
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SearchViewModel {
    pub query: String,
    pub results: Vec<SearchHit>,
    pub total: usize,
}

impl SearchViewModel {
    pub fn new() -> Self {
        Self {
            query: String::new(),
            results: Vec::new(),
            total: 0,
        }
    }

    pub fn set_query(&mut self, query: &str) {
        self.query = query.to_string();
    }
}

impl Default for SearchViewModel {
    fn default() -> Self {
        Self::new()
    }
}
