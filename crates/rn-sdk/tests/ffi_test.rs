//! FFI 型別測試

use rn_sdk::ffi::{FfiResult, FfiSearchResult};

#[test]
fn ffi_result_ok_is_success() {
    let r = FfiResult::ok("data".to_string());
    assert!(r.is_ok);
    assert!(r.error_msg.is_empty());
}

#[test]
fn ffi_result_err_has_message() {
    let r = FfiResult::err("failed".to_string());
    assert!(!r.is_ok);
    assert_eq!(r.error_msg, "failed");
}

#[test]
fn ffi_search_result_has_correct_total() {
    let hits = vec!["a".to_string(), "b".to_string(), "c".to_string()];
    let result = FfiSearchResult::new(hits);
    assert_eq!(result.total, 3);
}
