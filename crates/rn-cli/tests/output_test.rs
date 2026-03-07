//! Output 格式化測試

use rn_cli::output::*;

#[test]
fn search_text_format_contains_path_and_snippet() {
    let results = vec![SearchOutput {
        path: "docs/readme.md".to_string(),
        score: 1.5,
        snippet: Some("hello world".to_string()),
    }];
    let text = format_search_text(&results);
    assert!(text.contains("docs/readme.md"));
    assert!(text.contains("hello world"));
}

#[test]
fn search_json_format_is_valid_json() {
    let results = vec![SearchOutput {
        path: "test.txt".to_string(),
        score: 2.0,
        snippet: Some("snippet text".to_string()),
    }];
    let json_str = format_search_json(&results);
    let parsed: serde_json::Value = serde_json::from_str(&json_str).unwrap();
    assert!(parsed.is_array());
    assert_eq!(parsed[0]["path"], "test.txt");
}

#[test]
fn stats_text_format_contains_totals() {
    let stats = StatsOutput {
        total_docs: 42,
        index_size_bytes: 1024,
    };
    let text = format_stats_text(&stats);
    assert!(text.contains("42"));
    assert!(text.contains("1024"));
}

#[test]
fn stats_json_format_is_valid_json() {
    let stats = StatsOutput {
        total_docs: 42,
        index_size_bytes: 1024,
    };
    let json_str = format_stats_json(&stats);
    let parsed: serde_json::Value = serde_json::from_str(&json_str).unwrap();
    assert_eq!(parsed["total_docs"], 42);
    assert_eq!(parsed["index_size_bytes"], 1024);
}
