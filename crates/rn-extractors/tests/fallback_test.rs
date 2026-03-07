//! FallbackExtractor 行為測試

mod common;

use rn_extractors::fallback::FallbackExtractor;
use rn_extractors::{CostProfile, Extractor};

#[test]
fn supports_any_mime_type() {
    assert!(FallbackExtractor.supports("anything/whatever"));
    assert!(FallbackExtractor.supports("application/octet-stream"));
    assert!(FallbackExtractor.supports("text/plain"));
}

#[test]
fn reads_valid_utf8_file() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "unknown.xyz", b"some text content");

    let result = FallbackExtractor.extract(&path).unwrap();
    assert_eq!(result.raw_text, "some text content");
}

#[test]
fn handles_non_utf8_file_with_warning() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "binary.bin", &[0xFF, 0xFE, 0x00, 0x80, 0x90]);

    let result = FallbackExtractor.extract(&path).unwrap();
    assert!(!result.warnings.is_empty(), "非 UTF-8 應產生 warning");
}

#[test]
fn cost_profile_is_cheap() {
    assert_eq!(FallbackExtractor.cost_profile(), CostProfile::Cheap);
}
