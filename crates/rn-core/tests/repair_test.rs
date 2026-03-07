//! RepairPlan 診斷測試

use rn_core::repair::{Issue, RepairAction, RepairPlan};

#[test]
fn empty_issues_returns_empty_plan() {
    let plan = RepairPlan::diagnose(&[]);
    assert!(plan.actions.is_empty());
}

#[test]
fn stale_lock_suggests_remove_lock() {
    let plan = RepairPlan::diagnose(&[Issue::StaleLock]);
    assert!(plan.actions.contains(&RepairAction::RemoveLock));
}

#[test]
fn orphan_state_suggests_reset_state() {
    let plan = RepairPlan::diagnose(&[Issue::OrphanState]);
    assert!(plan.actions.contains(&RepairAction::ResetState));
}
