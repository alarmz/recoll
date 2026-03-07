//! FsWatcher 整合測試

use rn_core::task::OperationType;
use rn_watcher::watcher::FsWatcher;
use std::fs;
use std::time::Duration;

#[test]
fn detects_file_creation() {
    let dir = tempfile::tempdir().unwrap();
    let watcher = FsWatcher::new(dir.path()).unwrap();

    // 等待 watcher 就緒
    std::thread::sleep(Duration::from_millis(100));

    fs::write(dir.path().join("new.txt"), "hello").unwrap();

    let event = watcher.recv_timeout(Duration::from_secs(5));
    assert!(event.is_some(), "should receive a create event");
    let event = event.unwrap();
    assert!(
        event.operation == OperationType::Create || event.operation == OperationType::Update,
        "should be Create or Update, got {:?}",
        event.operation
    );
}

#[test]
fn detects_file_modification() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("exist.txt");
    fs::write(&path, "original").unwrap();

    let watcher = FsWatcher::new(dir.path()).unwrap();
    std::thread::sleep(Duration::from_millis(100));

    fs::write(&path, "modified content").unwrap();

    let event = watcher.recv_timeout(Duration::from_secs(5));
    assert!(event.is_some(), "should receive a modify event");
    let event = event.unwrap();
    assert_eq!(event.operation, OperationType::Update);
}

#[test]
fn detects_file_deletion() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("exist.txt");
    fs::write(&path, "data").unwrap();

    let watcher = FsWatcher::new(dir.path()).unwrap();
    std::thread::sleep(Duration::from_millis(100));

    fs::remove_file(&path).unwrap();

    // May need to drain multiple events to find the delete
    let mut found_delete = false;
    for _ in 0..10 {
        if let Some(event) = watcher.recv_timeout(Duration::from_secs(2)) {
            if event.operation == OperationType::Delete {
                found_delete = true;
                break;
            }
        } else {
            break;
        }
    }
    assert!(found_delete, "should receive a delete event");
}
