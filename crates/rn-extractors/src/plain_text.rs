use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

pub struct PlainTextExtractor;

impl Extractor for PlainTextExtractor {
    fn name(&self) -> &str {
        "PlainText"
    }

    fn supports(&self, mime_type: &str) -> bool {
        mime_type == "text/plain"
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Cheap
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let bytes = std::fs::read(path)?;
        let text = strip_bom(&bytes);
        Ok(make_result(text, ExtractionMethod::Native))
    }
}

fn strip_bom(bytes: &[u8]) -> String {
    if bytes.starts_with(&[0xEF, 0xBB, 0xBF]) {
        String::from_utf8_lossy(&bytes[3..]).into_owned()
    } else {
        String::from_utf8_lossy(bytes).into_owned()
    }
}
