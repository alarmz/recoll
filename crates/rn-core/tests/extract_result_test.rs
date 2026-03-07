//! 抽取結果型別行為測試

use rn_core::extract::{ExtractResult, ExtractWarning, ExtractionMethod, Language};

#[test]
fn extract_result_holds_raw_text() {
    let r = ExtractResult {
        raw_text: "Hello World".into(),
        title: None,
        summary_hint: None,
        detected_language: None,
        page_count: None,
        sheet_names: vec![],
        attachments: vec![],
        warnings: vec![],
        extraction_time_ms: 10,
        extraction_method: ExtractionMethod::Native,
    };
    assert_eq!(r.raw_text, "Hello World");
    assert!(r.warnings.is_empty());
}

#[test]
fn all_language_variants_exist() {
    let _e = Language::En;
    let _tw = Language::ZhTw;
    let _cn = Language::ZhCn;
    let _ja = Language::Ja;
    let _ko = Language::Ko;
    let _u = Language::Unknown;
}

#[test]
fn all_extraction_method_variants_exist() {
    let _n = ExtractionMethod::Native;
    let _e = ExtractionMethod::ExternalTool;
    let _o = ExtractionMethod::Ocr;
    let _f = ExtractionMethod::Fallback;
}

#[test]
fn all_extract_warning_variants_constructible() {
    let _p = ExtractWarning::PartialContent {
        reason: "test".into(),
    };
    let _e = ExtractWarning::EncodingIssue { chars_replaced: 3 };
    let _t = ExtractWarning::TruncatedAt { bytes: 1_000_000 };
    let _o = ExtractWarning::OcrUsed;
}
