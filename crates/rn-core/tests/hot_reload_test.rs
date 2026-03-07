//! HotReload 可熱更新判斷測試

use rn_core::config::is_hot_reloadable;

#[test]
fn log_level_is_hot_reloadable() {
    assert!(is_hot_reloadable("log_level"));
}

#[test]
fn max_workers_is_not_hot_reloadable() {
    assert!(!is_hot_reloadable("max_workers"));
}
