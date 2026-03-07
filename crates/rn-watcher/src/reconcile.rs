use std::collections::{BTreeSet, HashMap};
use std::path::PathBuf;
use std::time::SystemTime;

/// 檔案快照資訊
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct FileSnapshot {
    pub mtime: SystemTime,
    pub size: u64,
}

/// 校正結果
#[derive(Debug, Default)]
pub struct ReconcileResult {
    pub added: BTreeSet<PathBuf>,
    pub removed: BTreeSet<PathBuf>,
    pub modified: BTreeSet<PathBuf>,
}

/// 比較磁碟狀態與已知狀態，找出差異
pub fn reconcile(
    disk: &HashMap<PathBuf, FileSnapshot>,
    known: &HashMap<PathBuf, FileSnapshot>,
) -> ReconcileResult {
    let mut result = ReconcileResult::default();

    for (path, disk_snap) in disk {
        match known.get(path) {
            None => {
                result.added.insert(path.clone());
            }
            Some(known_snap) if known_snap != disk_snap => {
                result.modified.insert(path.clone());
            }
            Some(_) => {} // unchanged
        }
    }

    for path in known.keys() {
        if !disk.contains_key(path) {
            result.removed.insert(path.clone());
        }
    }

    result
}
