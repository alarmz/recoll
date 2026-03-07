use serde::{Deserialize, Serialize};

/// 預覽類型
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum PreviewVariant {
    Text,
    Image,
    Unsupported,
}

/// 預覽資料
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PreviewData {
    pub variant: PreviewVariant,
    pub content: String,
    pub mime: String,
}

impl PreviewData {
    /// 從 MIME 類型建立預覽
    pub fn new(mime: &str, content: &str) -> Self {
        let variant = if mime.starts_with("text/") {
            PreviewVariant::Text
        } else if mime.starts_with("image/") {
            PreviewVariant::Image
        } else {
            PreviewVariant::Unsupported
        };
        Self {
            variant,
            content: content.to_string(),
            mime: mime.to_string(),
        }
    }

    /// 文字預覽便利建構
    pub fn text(content: &str) -> Self {
        Self::new("text/plain", content)
    }
}
