use serde::{Deserialize, Serialize};

use crate::product::ProductInfo;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WixConfig {
    pub product: ProductInfo,
    pub manufacturer: String,
    pub upgrade_code: String,
}

impl Default for WixConfig {
    fn default() -> Self {
        Self {
            product: ProductInfo::default(),
            manufacturer: "Recoll Project".to_string(),
            upgrade_code: "recoll-next-default-upgrade-code".to_string(),
        }
    }
}

impl WixConfig {
    pub fn product_name(&self) -> &str {
        &self.product.name
    }

    pub fn install_dir(&self) -> String {
        format!(r"C:\Program Files\{}", self.product.name)
    }
}
