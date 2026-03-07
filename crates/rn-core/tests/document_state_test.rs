//! 文件狀態機轉換行為測試

use rn_core::state::{can_transition, DocumentState};

// --- 合法轉換 ---

#[test]
fn discovered_can_transition_to_queued() {
    assert!(can_transition(
        &DocumentState::Discovered,
        &DocumentState::Queued
    ));
}

#[test]
fn queued_can_transition_to_extracting() {
    assert!(can_transition(
        &DocumentState::Queued,
        &DocumentState::Extracting
    ));
}

#[test]
fn extracting_can_transition_to_extracted() {
    assert!(can_transition(
        &DocumentState::Extracting,
        &DocumentState::Extracted
    ));
}

#[test]
fn extracted_can_transition_to_normalized() {
    assert!(can_transition(
        &DocumentState::Extracted,
        &DocumentState::Normalized
    ));
}

#[test]
fn normalized_can_transition_to_indexed() {
    assert!(can_transition(
        &DocumentState::Normalized,
        &DocumentState::Indexed
    ));
}

#[test]
fn indexed_can_transition_to_stale() {
    assert!(can_transition(
        &DocumentState::Indexed,
        &DocumentState::Stale
    ));
}

#[test]
fn stale_can_transition_to_queued() {
    assert!(can_transition(
        &DocumentState::Stale,
        &DocumentState::Queued
    ));
}

#[test]
fn failed_can_retry_to_queued() {
    let failed = DocumentState::Failed {
        error: "timeout".into(),
        retry_count: 1,
    };
    assert!(can_transition(&failed, &DocumentState::Queued));
}

#[test]
fn any_state_can_transition_to_failed() {
    let failed = DocumentState::Failed {
        error: "crash".into(),
        retry_count: 0,
    };
    assert!(can_transition(&DocumentState::Extracting, &failed));
}

#[test]
fn indexed_can_transition_to_deleted() {
    assert!(can_transition(
        &DocumentState::Indexed,
        &DocumentState::Deleted
    ));
}

#[test]
fn stale_can_transition_to_deleted() {
    assert!(can_transition(
        &DocumentState::Stale,
        &DocumentState::Deleted
    ));
}

// --- 不合法轉換 ---

#[test]
fn discovered_cannot_skip_to_indexed() {
    assert!(!can_transition(
        &DocumentState::Discovered,
        &DocumentState::Indexed
    ));
}

#[test]
fn deleted_cannot_transition_to_queued() {
    assert!(!can_transition(
        &DocumentState::Deleted,
        &DocumentState::Queued
    ));
}

#[test]
fn indexed_cannot_go_back_to_discovered() {
    assert!(!can_transition(
        &DocumentState::Indexed,
        &DocumentState::Discovered
    ));
}
