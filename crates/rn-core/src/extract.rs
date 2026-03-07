use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExtractResult {
    pub raw_text: String,
    pub title: Option<String>,
    pub summary_hint: Option<String>,
    pub detected_language: Option<Language>,
    pub page_count: Option<u32>,
    pub sheet_names: Vec<String>,
    pub attachments: Vec<AttachmentMeta>,
    pub warnings: Vec<ExtractWarning>,
    pub extraction_time_ms: u64,
    pub extraction_method: ExtractionMethod,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Language {
    En,
    ZhTw,
    ZhCn,
    Ja,
    Ko,
    Unknown,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ExtractionMethod {
    Native,
    ExternalTool,
    Ocr,
    Fallback,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AttachmentMeta {
    pub name: String,
    pub mime_type: Option<String>,
    pub size: Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ExtractWarning {
    PartialContent { reason: String },
    EncodingIssue { chars_replaced: u32 },
    TruncatedAt { bytes: u64 },
    OcrUsed,
}
