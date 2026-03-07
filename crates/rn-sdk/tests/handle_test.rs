//! SDK Handle 生命週期測試

use rn_sdk::handle::{RecollNext, SdkConfig};

#[test]
fn sdk_config_default_has_reasonable_values() {
    let cfg = SdkConfig::default();
    assert_eq!(cfg.bind_address, "127.0.0.1");
    assert_eq!(cfg.port, 9312);
    assert!(!cfg.data_dir.as_os_str().is_empty());
}

#[test]
fn open_returns_handle() {
    let cfg = SdkConfig::default();
    let handle = RecollNext::open(cfg).unwrap();
    assert!(handle.is_open());
}

#[test]
fn close_sets_not_open() {
    let cfg = SdkConfig::default();
    let mut handle = RecollNext::open(cfg).unwrap();
    handle.close();
    assert!(!handle.is_open());
}
