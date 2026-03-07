use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::time::SystemTime;
use uuid::Uuid;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IndexTask {
    pub task_id: Uuid,
    pub file_path: PathBuf,
    pub file_version: FileVersion,
    pub priority: TaskPriority,
    pub operation: OperationType,
    pub source: TaskSource,
    pub scheduled_at: SystemTime,
    pub retry_count: u8,
}

impl IndexTask {
    pub fn new(
        file_path: PathBuf,
        file_version: FileVersion,
        priority: TaskPriority,
        operation: OperationType,
        source: TaskSource,
    ) -> Self {
        Self {
            task_id: Uuid::new_v4(),
            file_path,
            file_version,
            priority,
            operation,
            source,
            scheduled_at: SystemTime::now(),
            retry_count: 0,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct FileVersion {
    pub mtime: SystemTime,
    pub size: u64,
    pub hash: Option<[u8; 32]>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum TaskPriority {
    High = 0,
    Normal = 1,
    Low = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum OperationType {
    Create,
    Update,
    Delete,
    Verify,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TaskSource {
    InitialScan,
    Watcher,
    Manual,
    Retry,
    Reconcile,
}
