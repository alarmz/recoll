//! LockFile 管理測試

use rn_core::lock::LockFile;

#[test]
fn acquire_creates_lock_file() {
    let dir = tempfile::tempdir().unwrap();
    let lock = LockFile::acquire(dir.path()).unwrap();
    assert!(LockFile::lock_path(dir.path()).exists());
    lock.release().unwrap();
}

#[test]
fn acquire_twice_returns_error() {
    let dir = tempfile::tempdir().unwrap();
    let _lock = LockFile::acquire(dir.path()).unwrap();
    let result = LockFile::acquire(dir.path());
    assert!(result.is_err());
}

#[test]
fn release_removes_lock_file() {
    let dir = tempfile::tempdir().unwrap();
    let lock = LockFile::acquire(dir.path()).unwrap();
    lock.release().unwrap();
    assert!(!LockFile::lock_path(dir.path()).exists());
}

#[test]
fn is_stale_returns_true_for_old_lock() {
    let dir = tempfile::tempdir().unwrap();
    // 手動寫入一個假的 lock 內容（PID=0, 很久以前的時間戳）
    std::fs::write(LockFile::lock_path(dir.path()), "0\n946684800").unwrap();
    assert!(LockFile::is_stale(dir.path()));
}
