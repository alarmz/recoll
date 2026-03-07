use serde::Serialize;
use std::fmt::Write;

/// 格式化 trait — 支援 text 和 JSON 輸出
pub trait Formattable: Serialize {
    fn to_text(&self) -> String;

    fn to_json(&self) -> String {
        serde_json::to_string_pretty(self).unwrap_or_default()
    }
}

/// 搜尋結果輸出
#[derive(Debug, Clone, Serialize)]
pub struct SearchOutput {
    pub path: String,
    pub score: f32,
    pub snippet: Option<String>,
}

/// 搜尋結果列表的包裝
#[derive(Debug, Clone, Serialize)]
pub struct SearchResults(pub Vec<SearchOutput>);

/// 統計輸出
#[derive(Debug, Clone, Serialize)]
pub struct StatsOutput {
    pub total_docs: u64,
    pub index_size_bytes: u64,
}

impl Formattable for SearchResults {
    fn to_text(&self) -> String {
        let mut buf = String::new();
        for r in &self.0 {
            let _ = writeln!(buf, "{} (score: {:.2})", r.path, r.score);
            if let Some(ref s) = r.snippet {
                let _ = writeln!(buf, "  {s}");
            }
        }
        buf
    }
}

impl Formattable for StatsOutput {
    fn to_text(&self) -> String {
        format!(
            "Total documents: {}\nIndex size: {} bytes",
            self.total_docs, self.index_size_bytes
        )
    }
}

// Backward-compatible free functions
pub fn format_search_text(results: &[SearchOutput]) -> String {
    SearchResults(results.to_vec()).to_text()
}

pub fn format_search_json(results: &[SearchOutput]) -> String {
    serde_json::to_string_pretty(results).unwrap_or_default()
}

pub fn format_stats_text(stats: &StatsOutput) -> String {
    stats.to_text()
}

pub fn format_stats_json(stats: &StatsOutput) -> String {
    stats.to_json()
}
