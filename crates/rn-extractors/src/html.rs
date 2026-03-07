use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

pub struct HtmlExtractor;

impl Extractor for HtmlExtractor {
    fn name(&self) -> &str {
        "Html"
    }

    fn supports(&self, mime_type: &str) -> bool {
        mime_type == "text/html" || mime_type == "application/xhtml+xml"
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Medium
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let content = std::fs::read_to_string(path)?;
        let title = extract_title(&content);
        let text = strip_tags(&content);
        let mut result = make_result(text, ExtractionMethod::Native);
        result.title = title;
        Ok(result)
    }
}

fn extract_title(html: &str) -> Option<String> {
    let lower = html.to_lowercase();
    let start = lower.find("<title>")?;
    let after = start + 7;
    let end = lower[after..].find("</title>")?;
    let title = html[after..after + end].trim().to_string();
    if title.is_empty() {
        None
    } else {
        Some(title)
    }
}

fn strip_tags(html: &str) -> String {
    let mut result = String::with_capacity(html.len());
    let mut in_tag = false;
    let mut in_script = false;
    let lower = html.to_lowercase();
    let chars: Vec<char> = html.chars().collect();
    let lower_chars: Vec<char> = lower.chars().collect();

    let mut i = 0;
    while i < chars.len() {
        if !in_tag && chars[i] == '<' {
            let remaining: String = lower_chars[i..].iter().take(10).collect();
            if remaining.starts_with("<script") || remaining.starts_with("<style") {
                in_script = true;
            }
            if remaining.starts_with("</script") || remaining.starts_with("</style") {
                in_script = false;
            }
            in_tag = true;
        } else if in_tag && chars[i] == '>' {
            in_tag = false;
        } else if !in_tag && !in_script {
            result.push(chars[i]);
        }
        i += 1;
    }

    result.split_whitespace().collect::<Vec<_>>().join(" ")
}
