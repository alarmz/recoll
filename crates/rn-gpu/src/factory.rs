use crate::backend::GpuBackend;
use crate::null_backend::NullBackend;

/// 建立最佳可用後端
///
/// 依優先順序偵測：CUDA → Vulkan → NullBackend fallback
pub fn create_best_available() -> Box<dyn GpuBackend> {
    #[cfg(feature = "cuda")]
    {
        // TODO: 偵測 CUDA 裝置並回傳 CudaBackend
    }

    #[cfg(feature = "vulkan")]
    {
        // TODO: 偵測 Vulkan 裝置並回傳 VulkanBackend
    }

    Box::new(NullBackend::new())
}
