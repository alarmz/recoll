//! 共用型別行為測試

use rn_core::types::{FileId, MimeType};

#[test]
fn file_id_wraps_and_returns_string() {
    let id = FileId::new("doc-001".into());
    assert_eq!(id.as_str(), "doc-001");
}

#[test]
fn mime_type_wraps_and_returns_string() {
    let mt = MimeType::new("application/pdf".into());
    assert_eq!(mt.as_str(), "application/pdf");
}
