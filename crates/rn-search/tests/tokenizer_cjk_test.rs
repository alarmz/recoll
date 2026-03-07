//! rn_cjk tokenizer 行為測試 (jieba 中文斷詞)

mod common;

#[test]
fn cjk_tokenizer_segments_chinese_text() {
    let tokens = common::tokenize("rn_cjk", "全文搜尋引擎");
    assert!(tokens.len() >= 2);
    let joined = tokens.join(" ");
    assert!(joined.contains("搜尋") || joined.contains("引擎") || joined.contains("全文"));
}

#[test]
fn cjk_tokenizer_handles_mixed_chinese_english() {
    let tokens = common::tokenize("rn_cjk", "Recoll搜尋");
    let has_english = tokens.iter().any(|t| t == "recoll");
    let has_chinese = tokens
        .iter()
        .any(|t| t.chars().any(|c| ('\u{4e00}'..='\u{9fff}').contains(&c)));
    assert!(has_english, "應包含小寫英文 token 'recoll'");
    assert!(has_chinese, "應包含中文 token");
}
