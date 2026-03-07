//! PlainTextExtractor 行為測試

mod common;

use rn_extractors::plain_text::PlainTextExtractor;
use rn_extractors::{CostProfile, Extractor};

#[test]
fn supports_text_plain() {
    assert!(PlainTextExtractor.supports("text/plain"));
}

#[test]
fn reads_utf8_plain_text_file() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "test.txt", "Hello, 世界!".as_bytes());

    let result = PlainTextExtractor.extract(&path).unwrap();
    assert_eq!(result.raw_text, "Hello, 世界!");
}

#[test]
fn detects_and_strips_utf8_bom() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "bom.txt", b"\xEF\xBB\xBFhello bom");

    let result = PlainTextExtractor.extract(&path).unwrap();
    assert_eq!(result.raw_text, "hello bom");
}

#[test]
fn cost_profile_is_cheap() {
    assert_eq!(PlainTextExtractor.cost_profile(), CostProfile::Cheap);
}
