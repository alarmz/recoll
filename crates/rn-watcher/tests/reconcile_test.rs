//! Reconciler 行為測試

use rn_watcher::reconcile::{reconcile, FileSnapshot};
use std::collections::{BTreeSet, HashMap};
use std::path::PathBuf;
use std::time::{Duration, SystemTime};

fn now() -> SystemTime {
    SystemTime::now()
}

fn snap(mtime: SystemTime, size: u64) -> FileSnapshot {
    FileSnapshot { mtime, size }
}

fn set_of(paths: &[&str]) -> BTreeSet<PathBuf> {
    paths.iter().map(PathBuf::from).collect()
}

#[test]
fn new_file_on_disk_detected_as_added() {
    let t = now();
    let mut disk = HashMap::new();
    disk.insert(PathBuf::from("a.txt"), snap(t, 100));
    disk.insert(PathBuf::from("b.txt"), snap(t, 200));

    let mut known = HashMap::new();
    known.insert(PathBuf::from("a.txt"), snap(t, 100));

    let result = reconcile(&disk, &known);
    assert_eq!(result.added, set_of(&["b.txt"]));
    assert!(result.removed.is_empty());
}

#[test]
fn missing_file_detected_as_removed() {
    let t = now();
    let mut disk = HashMap::new();
    disk.insert(PathBuf::from("a.txt"), snap(t, 100));

    let mut known = HashMap::new();
    known.insert(PathBuf::from("a.txt"), snap(t, 100));
    known.insert(PathBuf::from("c.txt"), snap(t, 300));

    let result = reconcile(&disk, &known);
    assert!(result.added.is_empty());
    assert_eq!(result.removed, set_of(&["c.txt"]));
}

#[test]
fn unchanged_file_not_in_any_diff() {
    let t = now();
    let mut disk = HashMap::new();
    disk.insert(PathBuf::from("a.txt"), snap(t, 100));

    let mut known = HashMap::new();
    known.insert(PathBuf::from("a.txt"), snap(t, 100));

    let result = reconcile(&disk, &known);
    assert!(result.added.is_empty());
    assert!(result.removed.is_empty());
    assert!(result.modified.is_empty());
}

#[test]
fn modified_file_detected_by_mtime() {
    let t = now();
    let later = t + Duration::from_secs(60);

    let mut disk = HashMap::new();
    disk.insert(PathBuf::from("a.txt"), snap(later, 150));

    let mut known = HashMap::new();
    known.insert(PathBuf::from("a.txt"), snap(t, 100));

    let result = reconcile(&disk, &known);
    assert!(result.added.is_empty());
    assert!(result.removed.is_empty());
    assert_eq!(result.modified, set_of(&["a.txt"]));
}
