//! 搜尋結果型別行為測試

use std::time::Duration;
use rn_core::search::{SourceType, MatchReason, SearchResponse};

#[test]
fn all_source_type_variants_exist() {
    let _f = SourceType::File;
    let _e = SourceType::Email;
    let _c = SourceType::Code;
    let _a = SourceType::Archive;
    let _u = SourceType::Unknown;
}

#[test]
fn all_match_reason_variants_exist() {
    let _fe = MatchReason::FilenameExact;
    let _fp = MatchReason::FilenamePrefix;
    let _cp = MatchReason::ContentPhrase;
    let _ck = MatchReason::ContentKeyword;
    let _tm = MatchReason::TitleMatch;
    let _cb = MatchReason::Combined { filename_score: 0.8, content_score: 0.6 };
}

#[test]
fn search_response_tracks_hit_counts_and_duration() {
    let resp = SearchResponse {
        results: vec![],
        total_hits: 5,
        metadata_hits: 2,
        fulltext_hits: 3,
        duration: Duration::from_millis(42),
    };
    assert_eq!(resp.total_hits, 5);
    assert_eq!(resp.duration, Duration::from_millis(42));
}
