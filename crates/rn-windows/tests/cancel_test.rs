//! CancellationToken 測試

use rn_windows::cancel::CancellationToken;

#[test]
fn token_starts_as_not_cancelled() {
    let token = CancellationToken::new();
    assert!(!token.is_cancelled());
}

#[test]
fn cancel_changes_state() {
    let token = CancellationToken::new();
    token.cancel();
    assert!(token.is_cancelled());
}

#[test]
fn cloned_token_receives_cancel() {
    let token = CancellationToken::new();
    let clone = token.clone();
    token.cancel();
    assert!(clone.is_cancelled());
}
