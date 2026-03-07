use serde::{Deserialize, Serialize};

use crate::product::ProductInfo;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Compression {
    Lzma2,
    Zip,
    None,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InnoConfig {
    pub product: ProductInfo,
    pub compression: Compression,
}

impl Default for InnoConfig {
    fn default() -> Self {
        Self {
            product: ProductInfo::default(),
            compression: Compression::Lzma2,
        }
    }
}

impl InnoConfig {
    pub fn app_name(&self) -> &str {
        &self.product.name
    }

    pub fn version(&self) -> &str {
        &self.product.version
    }

    pub fn output_base_filename(&self) -> String {
        format!(
            "{}-{}-win64-setup",
            self.product.name.to_lowercase().replace(' ', "-"),
            self.product.version
        )
    }
}
