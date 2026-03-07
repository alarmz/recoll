use rn_core::extract::ExtractWarning;
use unicode_normalization::UnicodeNormalization;

/// 文字正規化結果
pub struct NormalizeResult {
    pub text: String,
    pub warnings: Vec<ExtractWarning>,
}

/// 文字正規化：NFC + 空白壓縮 + 截斷
pub fn normalize_text(input: &str, max_chars: usize) -> NormalizeResult {
    let mut warnings = Vec::new();

    // Unicode NFC normalization
    let nfc: String = input.nfc().collect();

    // Collapse whitespace
    let collapsed: String = nfc.split_whitespace().collect::<Vec<_>>().join(" ");

    // Truncate if needed
    let text = if collapsed.chars().count() > max_chars {
        warnings.push(ExtractWarning::TruncatedAt {
            bytes: max_chars as u64,
        });
        collapsed.chars().take(max_chars).collect()
    } else {
        collapsed
    };

    NormalizeResult { text, warnings }
}
