//! ShellExtension 測試

use rn_windows::shell_ext::ShellExtension;
use std::path::Path;

#[test]
fn registry_path_contains_app_name() {
    let ext = ShellExtension::new(Path::new("C:\\Program Files\\RecollNext\\rn-cli.exe"));
    assert!(ext.registry_path().contains("RecollNext"));
}

#[test]
fn menu_text_is_search_with_recoll() {
    let ext = ShellExtension::new(Path::new("C:\\rn-cli.exe"));
    assert_eq!(ext.menu_text(), "Search with Recoll Next");
}

#[test]
fn command_line_contains_exe_and_placeholder() {
    let ext = ShellExtension::new(Path::new("C:\\bin\\rn-cli.exe"));
    let cmd = ext.command_line();
    assert!(cmd.contains("C:\\bin\\rn-cli.exe"));
    assert!(cmd.contains("%V"));
}
