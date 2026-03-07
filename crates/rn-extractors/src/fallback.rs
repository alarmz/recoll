use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractWarning, ExtractionMethod};
use std::path::Path;

pub struct FallbackExtractor;

impl Extractor for FallbackExtractor {
    fn name(&self) -> &str {
        "Fallback"
    }

    fn supports(&self, _mime_type: &str) -> bool {
        true
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Cheap
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let bytes = std::fs::read(path)?;
        match String::from_utf8(bytes) {
            Ok(text) => Ok(make_result(text, ExtractionMethod::Fallback)),
            Err(e) => {
                let bytes = e.into_bytes();
                let text = String::from_utf8_lossy(&bytes).into_owned();
                let mut result = make_result(text, ExtractionMethod::Fallback);
                result
                    .warnings
                    .push(ExtractWarning::EncodingIssue { chars_replaced: 1 });
                Ok(result)
            }
        }
    }
}
