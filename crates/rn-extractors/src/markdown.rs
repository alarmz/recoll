use crate::{make_result, CostProfile, Extractor};
use anyhow::Result;
use pulldown_cmark::{Event, Parser, TagEnd};
use rn_core::extract::{ExtractResult, ExtractionMethod};
use std::path::Path;

pub struct MarkdownExtractor;

impl Extractor for MarkdownExtractor {
    fn name(&self) -> &str {
        "Markdown"
    }

    fn supports(&self, mime_type: &str) -> bool {
        mime_type == "text/markdown" || mime_type == "text/x-markdown"
    }

    fn cost_profile(&self) -> CostProfile {
        CostProfile::Medium
    }

    fn extract(&self, path: &Path) -> Result<ExtractResult> {
        let content = std::fs::read_to_string(path)?;
        let text = strip_markdown(&content);
        Ok(make_result(text, ExtractionMethod::Native))
    }
}

fn strip_markdown(input: &str) -> String {
    let parser = Parser::new(input);
    let mut text = String::new();

    for event in parser {
        match event {
            Event::Text(t) | Event::Code(t) => {
                text.push_str(&t);
            }
            Event::End(TagEnd::Heading(_)) => {
                text.push('\n');
            }
            Event::End(TagEnd::Paragraph) | Event::End(TagEnd::Item) => {
                text.push('\n');
            }
            Event::SoftBreak | Event::HardBreak => {
                text.push('\n');
            }
            _ => {}
        }
    }
    text.trim().to_string()
}
