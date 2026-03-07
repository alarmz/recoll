//! DeviceInfo 測試

use rn_gpu::backend::{DeviceInfo, GpuBackend};
use rn_gpu::null_backend::NullBackend;

#[test]
fn null_backend_is_not_gpu() {
    let b = NullBackend::new();
    assert!(!b.device_info().is_gpu);
}

#[test]
fn device_info_display_contains_name() {
    let info = DeviceInfo {
        name: "CPU".to_string(),
        is_gpu: false,
        memory_bytes: 0,
    };
    let display = format!("{info}");
    assert!(display.contains("CPU"));
}
