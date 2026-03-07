use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

pub struct SourceCodeExtractor;

const SUPPORTED_MIMES: &[&str] = &[
    "text/x-rust",
    "text/x-python",
    "text/x-c",
    "text/x-c++",
    "text/x-java",
    "text/x-go",
    "text/javascript",
    "application/javascript",
    "text/typescript",
    "application/json",
    "text/x-shellscript",
    "text/x-ruby",
    "text/x-perl",
    "text/x-php",
    "text/x-lua",
    "text/x-swift",
    "text/x-kotlin",
    "text/x-scala",
    "text/x-csharp",
    "application/xml",
    "text/xml",
    "text/x-toml",
    "text/x-yaml",
];

impl Extractor for SourceCodeExtractor {
    fn name(&self) -> &str {
        "SourceCode"
    }

    fn supports(&self, mime_type: &str) -> bool {
        SUPPORTED_MIMES.contains(&mime_type)
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Cheap
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let content = std::fs::read_to_string(path)?;
        Ok(make_result(content, ExtractionMethod::Native))
    }
}
