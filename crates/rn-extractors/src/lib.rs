pub mod csv;
pub mod fallback;
pub mod html;
pub mod markdown;
pub mod plain_text;
pub mod registry;
pub mod source_code;

use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

/// 抽取成本等級
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum CostProfile {
    Cheap,
    Medium,
    Expensive,
}

/// 文件抽取器 trait
pub trait Extractor: Send + Sync {
    fn name(&self) -> &str;
    fn supports(&self, mime_type: &str) -> bool;
    fn cost_profile(&self) -> CostProfile;
    fn extract(&self, path: &Path) -> Result<ExtractResult>;
}

/// 建立基本的 ExtractResult
pub(crate) fn make_result(raw_text: String, method: ExtractionMethod) -> ExtractResult {
    ExtractResult {
        raw_text,
        title: None,
        summary_hint: None,
        detected_language: None,
        page_count: None,
        sheet_names: Vec::new(),
        attachments: Vec::new(),
        warnings: Vec::new(),
        extraction_time_ms: 0,
        extraction_method: method,
    }
}
