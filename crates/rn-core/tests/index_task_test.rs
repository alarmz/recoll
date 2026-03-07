//! 索引任務建構行為測試

use std::path::PathBuf;
use std::time::SystemTime;
use rn_core::task::{IndexTask, FileVersion, OperationType, TaskSource, TaskPriority};

#[test]
fn new_task_has_unique_uuid() {
    let fv = FileVersion { mtime: SystemTime::now(), size: 1024, hash: None };
    let task = IndexTask::new(
        PathBuf::from("test.pdf"), fv, TaskPriority::Normal,
        OperationType::Create, TaskSource::InitialScan,
    );
    assert!(!task.task_id.is_nil());
}

#[test]
fn new_task_starts_with_zero_retries() {
    let fv = FileVersion { mtime: SystemTime::now(), size: 1024, hash: None };
    let task = IndexTask::new(
        PathBuf::from("test.pdf"), fv, TaskPriority::Normal,
        OperationType::Create, TaskSource::InitialScan,
    );
    assert_eq!(task.retry_count, 0);
}

#[test]
fn new_task_records_scheduled_time() {
    let before = SystemTime::now();
    let fv = FileVersion { mtime: before, size: 512, hash: None };
    let task = IndexTask::new(
        PathBuf::from("test.txt"), fv, TaskPriority::High,
        OperationType::Update, TaskSource::Watcher,
    );
    assert!(task.scheduled_at >= before);
}

#[test]
fn two_tasks_have_different_uuids() {
    let fv = FileVersion { mtime: SystemTime::now(), size: 100, hash: None };
    let t1 = IndexTask::new(PathBuf::from("a.txt"), fv.clone(), TaskPriority::Low, OperationType::Create, TaskSource::Manual);
    let t2 = IndexTask::new(PathBuf::from("b.txt"), fv, TaskPriority::Low, OperationType::Create, TaskSource::Manual);
    assert_ne!(t1.task_id, t2.task_id);
}

#[test]
fn file_version_equal_when_mtime_and_size_match() {
    let now = SystemTime::now();
    let v1 = FileVersion { mtime: now, size: 1024, hash: None };
    let v2 = FileVersion { mtime: now, size: 1024, hash: None };
    assert_eq!(v1, v2);
}

#[test]
fn file_version_not_equal_when_size_differs() {
    let now = SystemTime::now();
    let v1 = FileVersion { mtime: now, size: 1024, hash: None };
    let v2 = FileVersion { mtime: now, size: 2048, hash: None };
    assert_ne!(v1, v2);
}

#[test]
fn file_version_hash_defaults_to_none() {
    let v = FileVersion { mtime: SystemTime::now(), size: 512, hash: None };
    assert!(v.hash.is_none());
}

#[test]
fn all_operation_types_exist() {
    let _c = OperationType::Create;
    let _u = OperationType::Update;
    let _d = OperationType::Delete;
    let _v = OperationType::Verify;
}

#[test]
fn all_task_sources_exist() {
    let _i = TaskSource::InitialScan;
    let _w = TaskSource::Watcher;
    let _m = TaskSource::Manual;
    let _r = TaskSource::Retry;
    let _rc = TaskSource::Reconcile;
}
