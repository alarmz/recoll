//! Extractor trait 與 CostProfile 行為測試

mod common;

use rn_extractors::CostProfile;

#[test]
fn cost_profile_ordering_cheap_less_than_medium_less_than_expensive() {
    assert!(CostProfile::Cheap < CostProfile::Medium);
    assert!(CostProfile::Medium < CostProfile::Expensive);
    assert!(CostProfile::Cheap < CostProfile::Expensive);
}

#[test]
fn extractor_trait_can_be_implemented_and_returns_extract_result() {
    use rn_extractors::plain_text::PlainTextExtractor;
    use rn_extractors::Extractor;

    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "test.txt", b"hello world");

    let extractor = PlainTextExtractor;
    let result = extractor.extract(&path).unwrap();
    assert!(result.raw_text.contains("hello world"));
}
