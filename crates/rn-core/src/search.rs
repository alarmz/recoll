use std::path::PathBuf;
use std::time::{Duration, SystemTime};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchResult {
    pub doc_id: String,
    pub file_path: PathBuf,
    pub filename: String,
    pub title: Option<String>,
    pub snippet: String,
    pub score: f32,
    pub match_reason: MatchReason,
    pub modified_at: SystemTime,
    pub file_size: u64,
    pub mime_type: String,
    pub source_type: SourceType,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MatchReason {
    FilenameExact,
    FilenamePrefix,
    ContentPhrase,
    ContentKeyword,
    TitleMatch,
    Combined { filename_score: f32, content_score: f32 },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SourceType {
    File,
    Email,
    Code,
    Archive,
    Unknown,
}

pub struct SearchResponse {
    pub results: Vec<SearchResult>,
    pub total_hits: usize,
    pub metadata_hits: usize,
    pub fulltext_hits: usize,
    pub duration: Duration,
}
