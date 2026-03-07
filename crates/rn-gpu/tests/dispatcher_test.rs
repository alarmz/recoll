//! GpuDispatcher 行為測試

use rn_gpu::dispatcher::GpuDispatcher;
use rn_gpu::null_backend::NullBackend;

#[test]
fn pending_below_min_batch_try_flush_returns_empty() {
    let mut d = GpuDispatcher::new(Box::new(NullBackend::new()), 4);
    d.push("a".to_string());
    d.push("b".to_string());
    assert_eq!(d.pending(), 2);
    let result = d.try_flush();
    assert!(result.is_empty());
}

#[test]
fn pending_at_min_batch_try_flush_returns_results() {
    let mut d = GpuDispatcher::new(Box::new(NullBackend::new()), 2);
    d.push("a".to_string());
    d.push("b".to_string());
    d.push("c".to_string());
    let result = d.try_flush();
    assert_eq!(result.len(), 3);
}

#[test]
fn force_flush_regardless_of_min_batch() {
    let mut d = GpuDispatcher::new(Box::new(NullBackend::new()), 10);
    d.push("only_one".to_string());
    let result = d.force_flush();
    assert_eq!(result.len(), 1);
    assert_eq!(result[0], "only_one");
}

#[test]
fn pending_is_zero_after_flush() {
    let mut d = GpuDispatcher::new(Box::new(NullBackend::new()), 2);
    d.push("a".to_string());
    d.push("b".to_string());
    d.try_flush();
    assert_eq!(d.pending(), 0);
}
