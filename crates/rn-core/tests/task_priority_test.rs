//! 任務優先級排序行為測試

use rn_core::task::TaskPriority;

#[test]
fn high_priority_sorts_before_normal() {
    assert!(TaskPriority::High < TaskPriority::Normal);
}

#[test]
fn normal_priority_sorts_before_low() {
    assert!(TaskPriority::Normal < TaskPriority::Low);
}

#[test]
fn sorting_produces_high_normal_low_order() {
    let mut priorities = vec![TaskPriority::Low, TaskPriority::High, TaskPriority::Normal];
    priorities.sort();
    assert_eq!(
        priorities,
        vec![TaskPriority::High, TaskPriority::Normal, TaskPriority::Low]
    );
}

#[test]
fn same_priority_is_equal() {
    assert_eq!(TaskPriority::Normal, TaskPriority::Normal);
}
