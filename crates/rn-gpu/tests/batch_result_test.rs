//! BatchResult 測試

use rn_gpu::backend::GpuBackend;
use rn_gpu::null_backend::NullBackend;

#[test]
fn batch_result_holds_multiple_items() {
    let b = NullBackend::new();
    let result = b.batch_preprocess(&["a", "b", "c"]);
    assert_eq!(result.len(), 3);
}

#[test]
fn empty_batch_returns_empty_result() {
    let b = NullBackend::new();
    let result = b.batch_preprocess(&[]);
    assert!(result.is_empty());
}
