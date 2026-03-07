/// 排序權重設定
pub struct RankingWeights {
    pub fulltext: f32,
    pub filename: f32,
    pub recency: f32,
    pub exact_phrase_boost: f32,
    pub title_match_boost: f32,
}

impl Default for RankingWeights {
    fn default() -> Self {
        Self {
            fulltext: 0.6,
            filename: 0.3,
            recency: 0.1,
            exact_phrase_boost: 2.0,
            title_match_boost: 1.5,
        }
    }
}

/// 計算檔名匹配分數 (0.0 ~ 1.0)
pub fn compute_filename_score(query_tokens: &[&str], filename: &str) -> f32 {
    let filename_lower = filename.to_lowercase();
    let stem = filename_lower.split('.').next().unwrap_or(&filename_lower);

    let mut matched = 0;
    for token in query_tokens {
        if stem.contains(&token.to_lowercase()) {
            matched += 1;
        }
    }

    if query_tokens.is_empty() {
        return 0.0;
    }

    let ratio = matched as f32 / query_tokens.len() as f32;
    // Exact match bonus
    if matched > 0 && stem == query_tokens.join("") {
        (ratio + 1.0) / 2.0 // boost for exact
    } else {
        ratio * 0.8
    }
}

/// 計算時間衰減分數 (0.0 ~ 1.0)，越近的檔案分數越高
pub fn compute_recency_score(modified_at: i64) -> f32 {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;

    let age_days = ((now - modified_at) as f64 / 86400.0).max(0.0);
    // Exponential decay: half-life ~90 days
    let score = (-age_days / 130.0).exp();
    (score as f32).clamp(0.0, 1.0)
}
