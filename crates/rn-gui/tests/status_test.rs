//! StatusInfo 測試

use rn_gui::status::{AppState, StatusInfo};

#[test]
fn default_status_is_idle() {
    let s = StatusInfo::default();
    assert_eq!(s.state, AppState::Idle);
    assert_eq!(s.indexed_count, 0);
}

#[test]
fn indexing_status_has_progress() {
    let s = StatusInfo {
        state: AppState::Indexing,
        indexed_count: 100,
        queue_size: 50,
        progress: 50,
    };
    assert_eq!(s.progress, 50);
}

#[test]
fn status_info_serializes() {
    let s = StatusInfo::default();
    let json = serde_json::to_string(&s).unwrap();
    assert!(json.contains("\"state\""));
}
