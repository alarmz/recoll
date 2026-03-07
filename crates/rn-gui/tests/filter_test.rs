//! FilterState 測試

use rn_gui::filter::FilterState;

#[test]
fn default_filter_has_no_conditions() {
    let f = FilterState::default();
    assert!(f.file_type.is_none());
    assert!(f.path_prefix.is_none());
    assert!(f.date_from.is_none());
}

#[test]
fn filter_with_file_type_is_active() {
    let f = FilterState {
        file_type: Some("pdf".to_string()),
        ..Default::default()
    };
    assert!(f.is_active());
}

#[test]
fn empty_filter_is_not_active() {
    let f = FilterState::default();
    assert!(!f.is_active());
}
