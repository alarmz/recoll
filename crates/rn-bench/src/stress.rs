use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StressConfig {
    pub file_count: usize,
    pub format_mix: Vec<String>,
    pub duration_secs: u64,
}

impl Default for StressConfig {
    fn default() -> Self {
        Self {
            file_count: 100_000,
            format_mix: vec![
                "text".to_string(),
                "html".to_string(),
                "pdf".to_string(),
                "docx".to_string(),
                "csv".to_string(),
            ],
            duration_secs: 600,
        }
    }
}

impl StressConfig {
    pub fn with_file_count(mut self, count: usize) -> Self {
        self.file_count = count;
        self
    }
}

impl fmt::Display for StressConfig {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Stress test: {} files, {} formats",
            self.file_count,
            self.format_mix.len()
        )
    }
}
