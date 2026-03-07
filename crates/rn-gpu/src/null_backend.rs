use crate::backend::{DeviceInfo, GpuBackend};

/// CPU fallback 後端
#[derive(Debug, Clone, Copy)]
pub struct NullBackend;

impl NullBackend {
    pub fn new() -> Self {
        NullBackend
    }
}

impl Default for NullBackend {
    fn default() -> Self {
        Self::new()
    }
}

impl GpuBackend for NullBackend {
    fn name(&self) -> &str {
        "null"
    }

    fn device_info(&self) -> DeviceInfo {
        DeviceInfo {
            name: "CPU".to_string(),
            is_gpu: false,
            memory_bytes: 0,
        }
    }

    fn batch_preprocess(&self, texts: &[&str]) -> Vec<String> {
        texts.iter().map(|t| t.to_string()).collect()
    }

    fn batch_embed(&self, texts: &[&str], dim: usize) -> Vec<Vec<f32>> {
        texts.iter().map(|_| vec![0.0_f32; dim]).collect()
    }
}
