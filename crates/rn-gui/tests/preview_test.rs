//! PreviewData 測試

use rn_gui::preview::{PreviewData, PreviewVariant};

#[test]
fn text_mime_returns_text_variant() {
    let p = PreviewData::new("text/plain", "hello");
    assert_eq!(p.variant, PreviewVariant::Text);
}

#[test]
fn unknown_mime_returns_unsupported() {
    let p = PreviewData::new("application/octet-stream", "");
    assert_eq!(p.variant, PreviewVariant::Unsupported);
}

#[test]
fn preview_data_serializes() {
    let p = PreviewData::text("content");
    let json = serde_json::to_string(&p).unwrap();
    assert!(json.contains("\"content\""));
}
