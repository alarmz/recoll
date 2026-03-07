//! SearchViewModel 測試

use rn_gui::search_vm::SearchViewModel;

#[test]
fn new_viewmodel_has_empty_results() {
    let vm = SearchViewModel::new();
    assert!(vm.results.is_empty());
    assert_eq!(vm.total, 0);
}

#[test]
fn set_query_updates_query() {
    let mut vm = SearchViewModel::new();
    vm.set_query("hello");
    assert_eq!(vm.query, "hello");
}

#[test]
fn viewmodel_serializes_to_json() {
    let vm = SearchViewModel::new();
    let json = serde_json::to_string(&vm).unwrap();
    assert!(json.contains("\"query\""));
    assert!(json.contains("\"results\""));
}
