use std::fmt;

/// GPU 裝置資訊
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeviceInfo {
    pub name: String,
    pub is_gpu: bool,
    pub memory_bytes: u64,
}

impl fmt::Display for DeviceInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{} (gpu={}, mem={})",
            self.name, self.is_gpu, self.memory_bytes
        )
    }
}

/// GPU 後端 trait
pub trait GpuBackend: Send + Sync {
    /// 後端名稱
    fn name(&self) -> &str;

    /// 裝置資訊
    fn device_info(&self) -> DeviceInfo;

    /// 批次預處理文字
    fn batch_preprocess(&self, texts: &[&str]) -> Vec<String>;

    /// 批次嵌入（產生向量）
    fn batch_embed(&self, texts: &[&str], dim: usize) -> Vec<Vec<f32>>;
}
