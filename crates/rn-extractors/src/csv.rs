use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

pub struct CsvExtractor;

impl Extractor for CsvExtractor {
    fn name(&self) -> &str {
        "Csv"
    }

    fn supports(&self, mime_type: &str) -> bool {
        mime_type == "text/csv" || mime_type == "text/tab-separated-values"
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Medium
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let content = std::fs::read_to_string(path)?;
        Ok(make_result(content, ExtractionMethod::Native))
    }
}
