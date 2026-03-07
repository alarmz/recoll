//! 排序權重行為測試

use rn_search::ranking::{compute_filename_score, compute_recency_score, RankingWeights};

// --- 預設權重 ---

#[test]
fn default_weights_have_correct_values() {
    let w = RankingWeights::default();
    assert!((w.fulltext - 0.6).abs() < f32::EPSILON);
    assert!((w.filename - 0.3).abs() < f32::EPSILON);
    assert!((w.recency - 0.1).abs() < f32::EPSILON);
    assert!((w.exact_phrase_boost - 2.0).abs() < f32::EPSILON);
    assert!((w.title_match_boost - 1.5).abs() < f32::EPSILON);
}

// --- compute_filename_score ---

#[test]
fn filename_score_exact_match_returns_high_score() {
    let score = compute_filename_score(&["main"], "main.rs");
    assert!(score > 0.8, "完全匹配應得高分, got {score}");
}

#[test]
fn filename_score_partial_match_returns_mid_score() {
    let score = compute_filename_score(&["main"], "main_helper.rs");
    assert!(
        score > 0.0 && score <= 1.0,
        "部分匹配應得中間分數, got {score}"
    );
}

#[test]
fn filename_score_no_match_returns_zero() {
    let score = compute_filename_score(&["xyz"], "main.rs");
    assert!(
        (score - 0.0).abs() < f32::EPSILON,
        "無匹配應得 0, got {score}"
    );
}

// --- compute_recency_score ---

#[test]
fn recency_score_recent_file_scores_higher_than_old() {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;
    let recent = compute_recency_score(now - 3600); // 1 小時前
    let old = compute_recency_score(now - 365 * 86400); // 1 年前
    assert!(recent > old, "近期檔案分數 {recent} 應高於舊檔案 {old}");
    assert!(recent > 0.0 && recent <= 1.0);
    assert!(old >= 0.0 && old <= 1.0);
}
