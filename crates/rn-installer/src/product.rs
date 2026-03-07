use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ProductInfo {
    pub name: String,
    pub version: String,
}

impl Default for ProductInfo {
    fn default() -> Self {
        Self {
            name: "Recoll Next".to_string(),
            version: "0.1.0".to_string(),
        }
    }
}
