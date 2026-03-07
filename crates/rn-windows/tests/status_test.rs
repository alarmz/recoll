//! ServiceStatus 測試

use rn_windows::status::ServiceStatus;

#[test]
fn parse_running() {
    assert_eq!(ServiceStatus::parse("running"), ServiceStatus::Running);
}

#[test]
fn parse_stopped() {
    assert_eq!(ServiceStatus::parse("stopped"), ServiceStatus::Stopped);
}

#[test]
fn parse_unknown_string() {
    assert_eq!(ServiceStatus::parse("xyz"), ServiceStatus::Unknown);
}
