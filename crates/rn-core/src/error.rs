use std::path::PathBuf;

#[derive(Debug, thiserror::Error)]
pub enum IndexError {
    #[error("extraction failed for {path}: {reason}")]
    Extraction { path: PathBuf, reason: String },

    #[error("storage error: {reason}")]
    Storage { reason: String },

    #[error("search error for query '{query}': {reason}")]
    Search { query: String, reason: String },

    #[error("io error: {0}")]
    Io(#[from] std::io::Error),
}
