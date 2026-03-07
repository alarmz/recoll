//! CsvExtractor 行為測試

mod common;

use rn_extractors::csv::CsvExtractor;
use rn_extractors::Extractor;

#[test]
fn supports_text_csv() {
    assert!(CsvExtractor.supports("text/csv"));
}

#[test]
fn reads_csv_file_content() {
    let dir = tempfile::tempdir().unwrap();
    let path = common::write_temp_file(&dir, "data.csv", b"name,age\nAlice,30\nBob,25\n");

    let result = CsvExtractor.extract(&path).unwrap();
    assert!(result.raw_text.contains("Alice"));
    assert!(result.raw_text.contains("Bob"));
}
