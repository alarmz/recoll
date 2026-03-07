use crate::backend::GpuBackend;

/// GPU 批次調度器
pub struct GpuDispatcher {
    backend: Box<dyn GpuBackend>,
    min_batch_size: usize,
    buffer: Vec<String>,
}

impl GpuDispatcher {
    pub fn new(backend: Box<dyn GpuBackend>, min_batch_size: usize) -> Self {
        Self {
            backend,
            min_batch_size,
            buffer: Vec::new(),
        }
    }

    /// 加入一筆待處理文字
    pub fn push(&mut self, text: String) {
        self.buffer.push(text);
    }

    /// 目前 pending 數量
    pub fn pending(&self) -> usize {
        self.buffer.len()
    }

    /// 嘗試 flush（達到 min_batch_size 才處理）
    pub fn try_flush(&mut self) -> Vec<String> {
        if self.buffer.len() >= self.min_batch_size {
            self.flush_internal()
        } else {
            Vec::new()
        }
    }

    /// 強制 flush（不論數量）
    pub fn force_flush(&mut self) -> Vec<String> {
        self.flush_internal()
    }

    /// 嘗試 embed（達到 min_batch_size 才處理）
    pub fn try_embed(&mut self, dim: usize) -> Vec<Vec<f32>> {
        if self.buffer.len() >= self.min_batch_size {
            self.embed_internal(dim)
        } else {
            Vec::new()
        }
    }

    /// 強制 embed（不論數量）
    pub fn force_embed(&mut self, dim: usize) -> Vec<Vec<f32>> {
        self.embed_internal(dim)
    }

    fn flush_internal(&mut self) -> Vec<String> {
        let texts: Vec<&str> = self.buffer.iter().map(|s| s.as_str()).collect();
        let result = self.backend.batch_preprocess(&texts);
        self.buffer.clear();
        result
    }

    fn embed_internal(&mut self, dim: usize) -> Vec<Vec<f32>> {
        let texts: Vec<&str> = self.buffer.iter().map(|s| s.as_str()).collect();
        let result = self.backend.batch_embed(&texts, dim);
        self.buffer.clear();
        result
    }
}
