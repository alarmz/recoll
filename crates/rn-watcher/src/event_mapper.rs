use notify::event::{ModifyKind, RenameMode};
use notify::EventKind;
use rn_core::task::OperationType;
use std::path::PathBuf;

/// 映射後的事件
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MappedEvent {
    pub path: PathBuf,
    pub operation: OperationType,
}

/// 將 notify EventKind 轉換為 OperationType
pub fn map_event(kind: &EventKind, paths: &[PathBuf]) -> Vec<MappedEvent> {
    match kind {
        EventKind::Create(_) => paths
            .iter()
            .map(|p| MappedEvent {
                path: p.clone(),
                operation: OperationType::Create,
            })
            .collect(),
        EventKind::Modify(ModifyKind::Name(RenameMode::Both)) if paths.len() >= 2 => {
            vec![
                MappedEvent {
                    path: paths[0].clone(),
                    operation: OperationType::Delete,
                },
                MappedEvent {
                    path: paths[1].clone(),
                    operation: OperationType::Create,
                },
            ]
        }
        EventKind::Modify(ModifyKind::Name(RenameMode::From)) => paths
            .iter()
            .map(|p| MappedEvent {
                path: p.clone(),
                operation: OperationType::Delete,
            })
            .collect(),
        EventKind::Modify(ModifyKind::Name(RenameMode::To)) => paths
            .iter()
            .map(|p| MappedEvent {
                path: p.clone(),
                operation: OperationType::Create,
            })
            .collect(),
        EventKind::Modify(_) => paths
            .iter()
            .map(|p| MappedEvent {
                path: p.clone(),
                operation: OperationType::Update,
            })
            .collect(),
        EventKind::Remove(_) => paths
            .iter()
            .map(|p| MappedEvent {
                path: p.clone(),
                operation: OperationType::Delete,
            })
            .collect(),
        _ => Vec::new(),
    }
}
