use crate::Extractor;
use anyhow::Result;
use rn_core::extract::ExtractResult;
use std::path::Path;

/// 抽取器註冊表
pub struct ExtractorRegistry {
    extractors: Vec<Box<dyn Extractor>>,
}

impl ExtractorRegistry {
    pub fn default_registry() -> Self {
        let extractors: Vec<Box<dyn Extractor>> = vec![
            Box::new(crate::plain_text::PlainTextExtractor),
            Box::new(crate::html::HtmlExtractor),
            Box::new(crate::markdown::MarkdownExtractor),
            Box::new(crate::source_code::SourceCodeExtractor),
            Box::new(crate::csv::CsvExtractor),
            Box::new(crate::fallback::FallbackExtractor),
        ];
        Self { extractors }
    }

    pub fn find_extractor(&self, mime_type: &str) -> Option<&dyn Extractor> {
        for ext in &self.extractors {
            if ext.name() != "Fallback" && ext.supports(mime_type) {
                return Some(ext.as_ref());
            }
        }
        self.extractors
            .iter()
            .find(|e| e.name() == "Fallback")
            .map(|e| e.as_ref())
    }

    pub fn extract_by_path(&self, path: &Path) -> Result<ExtractResult> {
        let mime = mime_guess::from_path(path)
            .first_or_octet_stream()
            .to_string();
        let extractor = self
            .find_extractor(&mime)
            .unwrap_or_else(|| self.extractors.last().unwrap().as_ref());
        extractor.extract(path)
    }

    pub fn extractor_count(&self) -> usize {
        self.extractors.len()
    }
}
