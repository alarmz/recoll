//! ExtractorRegistry 行為測試

mod common;

use rn_extractors::registry::ExtractorRegistry;

#[test]
fn default_registry_contains_all_extractors() {
    let registry = ExtractorRegistry::default_registry();
    assert!(
        registry.extractor_count() >= 6,
        "registry 應至少包含 6 個 extractor, got {}",
        registry.extractor_count()
    );
}

#[test]
fn find_extractor_returns_correct_extractor_for_text_plain() {
    let registry = ExtractorRegistry::default_registry();
    let ext = registry.find_extractor("text/plain").unwrap();
    assert_eq!(ext.name(), "PlainText");
}

#[test]
fn find_extractor_returns_fallback_for_unknown_mime() {
    let registry = ExtractorRegistry::default_registry();
    let ext = registry.find_extractor("application/octet-stream").unwrap();
    assert_eq!(ext.name(), "Fallback");
}

#[test]
fn extract_by_path_auto_selects_extractor() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "hello.txt", b"auto detected content");

    let registry = ExtractorRegistry::default_registry();
    let result = registry.extract_by_path(&path).unwrap();
    assert!(result.raw_text.contains("auto detected content"));
}
