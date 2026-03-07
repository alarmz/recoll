//! EventMapper 行為測試

use notify::event::{CreateKind, ModifyKind, RemoveKind, RenameMode};
use notify::EventKind;
use rn_core::task::OperationType;
use rn_watcher::event_mapper::map_event;
use std::path::PathBuf;

#[test]
fn create_event_maps_to_create() {
    let kind = EventKind::Create(CreateKind::File);
    let paths = vec![PathBuf::from("new.txt")];
    let events = map_event(&kind, &paths);
    assert_eq!(events.len(), 1);
    assert_eq!(events[0].operation, OperationType::Create);
    assert_eq!(events[0].path, PathBuf::from("new.txt"));
}

#[test]
fn modify_event_maps_to_update() {
    let kind = EventKind::Modify(ModifyKind::Data(notify::event::DataChange::Content));
    let paths = vec![PathBuf::from("exist.txt")];
    let events = map_event(&kind, &paths);
    assert_eq!(events.len(), 1);
    assert_eq!(events[0].operation, OperationType::Update);
}

#[test]
fn remove_event_maps_to_delete() {
    let kind = EventKind::Remove(RemoveKind::File);
    let paths = vec![PathBuf::from("old.txt")];
    let events = map_event(&kind, &paths);
    assert_eq!(events.len(), 1);
    assert_eq!(events[0].operation, OperationType::Delete);
}

#[test]
fn rename_produces_delete_and_create() {
    let kind = EventKind::Modify(ModifyKind::Name(RenameMode::Both));
    let paths = vec![PathBuf::from("old.txt"), PathBuf::from("new.txt")];
    let events = map_event(&kind, &paths);
    assert_eq!(events.len(), 2);
    assert_eq!(events[0].operation, OperationType::Delete);
    assert_eq!(events[0].path, PathBuf::from("old.txt"));
    assert_eq!(events[1].operation, OperationType::Create);
    assert_eq!(events[1].path, PathBuf::from("new.txt"));
}

#[test]
fn unknown_event_returns_empty() {
    let kind = EventKind::Other;
    let paths = vec![PathBuf::from("x.txt")];
    let events = map_event(&kind, &paths);
    assert!(events.is_empty());
}
