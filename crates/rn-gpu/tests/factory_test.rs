//! BackendFactory 測試

use rn_gpu::factory::create_best_available;

#[test]
fn fallback_to_null_backend() {
    let backend = create_best_available();
    assert_eq!(backend.name(), "null");
}

#[test]
fn factory_backend_can_preprocess() {
    let backend = create_best_available();
    let result = backend.batch_preprocess(&["hello"]);
    assert_eq!(result.len(), 1);
}
