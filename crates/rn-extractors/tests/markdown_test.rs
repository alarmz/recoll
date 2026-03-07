//! MarkdownExtractor 行為測試

mod common;

use rn_extractors::markdown::MarkdownExtractor;
use rn_extractors::Extractor;

#[test]
fn supports_text_markdown() {
    assert!(MarkdownExtractor.supports("text/markdown"));
}

#[test]
fn strips_markdown_syntax_keeps_text() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(
        &dir,
        "test.md",
        b"# Hello\n\n**bold** and *italic*\n\n- item1\n- item2",
    );

    let result = MarkdownExtractor.extract(&path).unwrap();
    assert!(result.raw_text.contains("Hello"));
    assert!(result.raw_text.contains("bold"));
    assert!(result.raw_text.contains("italic"));
    assert!(!result.raw_text.contains("**"));
    assert!(!result.raw_text.contains("# "));
}
