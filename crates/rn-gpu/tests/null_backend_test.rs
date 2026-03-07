//! NullBackend 行為測試

use rn_gpu::backend::GpuBackend;
use rn_gpu::null_backend::NullBackend;

#[test]
fn null_backend_name_is_null() {
    let b = NullBackend::new();
    assert_eq!(b.name(), "null");
}

#[test]
fn null_backend_device_info_not_gpu() {
    let b = NullBackend::new();
    let info = b.device_info();
    assert!(!info.is_gpu);
}

#[test]
fn batch_preprocess_returns_same_texts() {
    let b = NullBackend::new();
    let input = vec!["hello", "world", "test"];
    let result = b.batch_preprocess(&input);
    assert_eq!(result, vec!["hello", "world", "test"]);
}

#[test]
fn batch_embed_returns_zero_vectors() {
    let b = NullBackend::new();
    let input = vec!["a", "b"];
    let result = b.batch_embed(&input, 4);
    assert_eq!(result.len(), 2);
    assert_eq!(result[0], vec![0.0_f32; 4]);
    assert_eq!(result[1], vec![0.0_f32; 4]);
}
