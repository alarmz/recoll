//! TaskQueue 行為測試

use rn_core::task::TaskPriority;
use rn_indexer::queue::{PrioritizedTask, TaskQueue};
use std::path::PathBuf;

fn task(name: &str, priority: TaskPriority, seq: u64) -> PrioritizedTask {
    PrioritizedTask {
        path: PathBuf::from(name),
        priority,
        sequence: seq,
    }
}

#[test]
fn pop_returns_highest_priority_first() {
    let mut q = TaskQueue::new();
    q.push(task("low.txt", TaskPriority::Low, 1));
    q.push(task("high.txt", TaskPriority::High, 2));
    q.push(task("normal.txt", TaskPriority::Normal, 3));

    let first = q.pop().unwrap();
    assert_eq!(first.priority, TaskPriority::High);
    let second = q.pop().unwrap();
    assert_eq!(second.priority, TaskPriority::Normal);
    let third = q.pop().unwrap();
    assert_eq!(third.priority, TaskPriority::Low);
}

#[test]
fn pop_empty_queue_returns_none() {
    let mut q = TaskQueue::new();
    assert!(q.pop().is_none());
}

#[test]
fn len_tracks_push_and_pop() {
    let mut q = TaskQueue::new();
    assert_eq!(q.len(), 0);
    assert!(q.is_empty());

    q.push(task("a.txt", TaskPriority::Normal, 1));
    q.push(task("b.txt", TaskPriority::Normal, 2));
    q.push(task("c.txt", TaskPriority::Normal, 3));
    assert_eq!(q.len(), 3);

    q.pop();
    assert_eq!(q.len(), 2);
}

#[test]
fn same_priority_pops_in_fifo_order() {
    let mut q = TaskQueue::new();
    q.push(task("first.txt", TaskPriority::Normal, 1));
    q.push(task("second.txt", TaskPriority::Normal, 2));
    q.push(task("third.txt", TaskPriority::Normal, 3));

    let first = q.pop().unwrap();
    assert_eq!(first.path, PathBuf::from("first.txt"));
    let second = q.pop().unwrap();
    assert_eq!(second.path, PathBuf::from("second.txt"));
}
