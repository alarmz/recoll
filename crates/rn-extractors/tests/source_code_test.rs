//! SourceCodeExtractor 行為測試

mod common;

use rn_extractors::source_code::SourceCodeExtractor;
use rn_extractors::Extractor;

#[test]
fn supports_common_source_code_mime_types() {
    let ext = SourceCodeExtractor;
    assert!(ext.supports("text/x-rust"));
    assert!(ext.supports("text/x-python"));
    assert!(ext.supports("text/x-c"));
    assert!(ext.supports("text/javascript"));
    assert!(ext.supports("application/json"));
}

#[test]
fn reads_source_code_file_preserving_content() {
    let dir = tempfile::tempdir().unwrap();
    let content = b"fn main() {\n    println!(\"hello\");\n}\n";
    let path = common::write_temp_file(&dir, "main.rs", content);

    let result = SourceCodeExtractor.extract(&path).unwrap();
    assert_eq!(result.raw_text, std::str::from_utf8(content).unwrap());
}
