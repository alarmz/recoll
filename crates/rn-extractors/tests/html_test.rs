//! HtmlExtractor 行為測試

mod common;

use rn_extractors::html::HtmlExtractor;
use rn_extractors::Extractor;

#[test]
fn supports_text_html() {
    assert!(HtmlExtractor.supports("text/html"));
}

#[test]
fn strips_html_tags_keeps_text() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(
        &dir,
        "test.html",
        b"<html><body><p>hello</p><p>world</p></body></html>",
    );

    let result = HtmlExtractor.extract(&path).unwrap();
    assert!(result.raw_text.contains("hello"));
    assert!(result.raw_text.contains("world"));
    assert!(!result.raw_text.contains("<p>"));
}

#[test]
fn extracts_title_from_html() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(
        &dir,
        "titled.html",
        b"<html><head><title>My Page</title></head><body>content</body></html>",
    );

    let result = HtmlExtractor.extract(&path).unwrap();
    assert_eq!(result.title, Some("My Page".to_string()));
}
