//! TrayAction 測試

use rn_gui::tray::TrayAction;

#[test]
fn all_actions_contains_show_hide_quit() {
    let actions = TrayAction::all();
    assert!(actions.contains(&TrayAction::Show));
    assert!(actions.contains(&TrayAction::Hide));
    assert!(actions.contains(&TrayAction::Quit));
}

#[test]
fn show_label_is_show_window() {
    assert_eq!(TrayAction::Show.label(), "Show Window");
}

#[test]
fn from_id_quit_returns_some() {
    assert_eq!(TrayAction::from_id("quit"), Some(TrayAction::Quit));
}
