//! Debouncer 行為測試

use rn_core::task::OperationType;
use rn_watcher::debounce::Debouncer;
use std::path::PathBuf;
use std::time::Duration;

#[test]
fn first_event_is_recorded_as_pending() {
    let mut d = Debouncer::new(Duration::from_millis(200));
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    assert_eq!(d.pending_count(), 1);
}

#[test]
fn rapid_writes_within_window_are_collapsed() {
    let mut d = Debouncer::new(Duration::from_millis(200));
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    assert_eq!(d.pending_count(), 1);
}

#[test]
fn drain_ready_returns_expired_events() {
    let mut d = Debouncer::new(Duration::from_millis(10));
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    std::thread::sleep(Duration::from_millis(30));
    let events = d.drain_ready();
    assert_eq!(events.len(), 1);
    assert_eq!(events[0].path, PathBuf::from("a.txt"));
    assert_eq!(d.pending_count(), 0);
}

#[test]
fn different_files_are_independent() {
    let mut d = Debouncer::new(Duration::from_millis(200));
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    d.record(PathBuf::from("b.txt"), OperationType::Create);
    assert_eq!(d.pending_count(), 2);
}

#[test]
fn delete_event_overrides_pending_and_is_immediate() {
    let mut d = Debouncer::new(Duration::from_millis(5000));
    d.record(PathBuf::from("a.txt"), OperationType::Update);
    d.record(PathBuf::from("a.txt"), OperationType::Delete);
    // Delete should be immediately drainable even within window
    let events = d.drain_ready();
    assert_eq!(events.len(), 1);
    assert_eq!(events[0].operation, OperationType::Delete);
}
