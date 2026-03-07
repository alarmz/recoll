//! IoThrottle 行為測試

use rn_indexer::throttle::IoThrottle;

#[test]
fn off_mode_has_zero_delay() {
    assert_eq!(IoThrottle::Off.delay_ms(), 0);
}

#[test]
fn gentle_mode_has_positive_delay() {
    assert!(IoThrottle::Gentle.delay_ms() > 0);
}

#[test]
fn aggressive_delay_greater_than_gentle() {
    assert!(IoThrottle::Aggressive.delay_ms() > IoThrottle::Gentle.delay_ms());
}
