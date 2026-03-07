//! 錯誤型別行為測試

use rn_core::error::IndexError;

#[test]
fn index_error_implements_std_error_and_display() {
    let err = IndexError::Extraction { path: "test.pdf".into(), reason: "corrupted".into() };
    let _: &dyn std::error::Error = &err;
    let msg = format!("{}", err);
    assert!(!msg.is_empty());
}

#[test]
fn index_error_has_extraction_variant() {
    let _e = IndexError::Extraction { path: "a.pdf".into(), reason: "fail".into() };
}

#[test]
fn index_error_has_storage_variant() {
    let _s = IndexError::Storage { reason: "db locked".into() };
}

#[test]
fn index_error_has_search_variant() {
    let _s = IndexError::Search { query: "hello".into(), reason: "bad syntax".into() };
}

#[test]
fn index_error_has_io_variant() {
    let _i = IndexError::Io(std::io::Error::new(std::io::ErrorKind::NotFound, "missing"));
}
