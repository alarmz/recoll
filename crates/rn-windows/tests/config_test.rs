//! ServiceConfig 測試

use rn_windows::config::ServiceConfig;

#[test]
fn default_service_name_is_recoll_next() {
    let cfg = ServiceConfig::default();
    assert_eq!(cfg.name, "RecollNext");
}

#[test]
fn default_binary_name_is_rn_cli_exe() {
    let cfg = ServiceConfig::default();
    assert_eq!(cfg.binary_name, "rn-cli.exe");
}

#[test]
fn display_name_contains_recoll() {
    let cfg = ServiceConfig::default();
    assert!(cfg.display_name.contains("Recoll"));
}
