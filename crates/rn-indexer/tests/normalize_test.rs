//! Normalize 行為測試

use rn_indexer::normalize::normalize_text;

#[test]
fn unicode_nfc_normalization() {
    // e + combining acute accent → precomposed e-acute
    let input = "caf\u{0065}\u{0301}";
    let result = normalize_text(input, 1000);
    assert_eq!(result.text, "caf\u{00e9}");
}

#[test]
fn truncates_at_max_chars_with_warning() {
    let input = "a".repeat(500);
    let result = normalize_text(&input, 100);
    assert!(result.text.len() <= 100);
    assert!(!result.warnings.is_empty(), "截斷應產生 warning");
}

#[test]
fn collapses_whitespace() {
    let input = "hello   world\t\tnew\n\nline";
    let result = normalize_text(input, 1000);
    assert_eq!(result.text, "hello world new line");
}
