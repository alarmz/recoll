//! ServiceInstaller 測試

use rn_windows::config::ServiceConfig;
use rn_windows::installer;

#[test]
fn install_args_contain_create_and_name() {
    let cfg = ServiceConfig::default();
    let args = installer::install_args(&cfg);
    let joined = args.join(" ");
    assert!(joined.contains("create"));
    assert!(joined.contains("RecollNext"));
}

#[test]
fn uninstall_args_contain_delete_and_name() {
    let cfg = ServiceConfig::default();
    let args = installer::uninstall_args(&cfg);
    let joined = args.join(" ");
    assert!(joined.contains("delete"));
    assert!(joined.contains("RecollNext"));
}

#[test]
fn start_and_stop_args() {
    let cfg = ServiceConfig::default();
    let start = installer::start_args(&cfg);
    let stop = installer::stop_args(&cfg);
    assert!(start.join(" ").contains("start"));
    assert!(stop.join(" ").contains("stop"));
}
