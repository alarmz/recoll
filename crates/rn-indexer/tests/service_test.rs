//! ServiceState 行為測試

use rn_indexer::service::ServiceState;

#[test]
fn idle_to_running_is_valid() {
    assert!(ServiceState::Idle.can_transition_to(ServiceState::Running));
}

#[test]
fn running_to_paused_is_valid() {
    assert!(ServiceState::Running.can_transition_to(ServiceState::Paused));
}

#[test]
fn paused_to_running_is_valid() {
    assert!(ServiceState::Paused.can_transition_to(ServiceState::Running));
}

#[test]
fn running_to_stopped_is_valid() {
    assert!(ServiceState::Running.can_transition_to(ServiceState::Stopped));
}

#[test]
fn stopped_to_running_is_invalid() {
    assert!(!ServiceState::Stopped.can_transition_to(ServiceState::Running));
}

#[test]
fn idle_to_paused_is_invalid() {
    assert!(!ServiceState::Idle.can_transition_to(ServiceState::Paused));
}
