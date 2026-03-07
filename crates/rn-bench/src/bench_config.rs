use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OutputFormat {
    Json,
    Text,
    Csv,
}

impl fmt::Display for OutputFormat {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            OutputFormat::Json => write!(f, "json"),
            OutputFormat::Text => write!(f, "text"),
            OutputFormat::Csv => write!(f, "csv"),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BenchConfig {
    pub iterations: u32,
    pub warmup_secs: u32,
    pub output_format: OutputFormat,
}

impl Default for BenchConfig {
    fn default() -> Self {
        Self {
            iterations: 100,
            warmup_secs: 3,
            output_format: OutputFormat::Json,
        }
    }
}
