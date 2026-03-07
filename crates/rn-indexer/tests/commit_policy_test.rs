//! CommitPolicy 行為測試

use rn_indexer::commit_policy::{CommitPolicy, CommitTracker};
use std::time::Duration;

#[test]
fn by_count_triggers_at_threshold() {
    let mut tracker = CommitTracker::new(CommitPolicy::ByCount(10));
    for _ in 0..10 {
        tracker.record_document();
    }
    assert!(tracker.should_commit());
}

#[test]
fn by_count_does_not_trigger_below_threshold() {
    let mut tracker = CommitTracker::new(CommitPolicy::ByCount(10));
    for _ in 0..5 {
        tracker.record_document();
    }
    assert!(!tracker.should_commit());
}

#[test]
fn by_time_triggers_after_duration() {
    // Use a very short duration so the test passes immediately
    let mut tracker = CommitTracker::new(CommitPolicy::ByTime(Duration::from_millis(0)));
    tracker.record_document();
    std::thread::sleep(Duration::from_millis(1));
    assert!(tracker.should_commit());
}
