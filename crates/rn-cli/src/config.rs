use anyhow::Result;
use serde::{Deserialize, Serialize};
use std::path::Path;

/// CLI 設定
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CliConfig {
    pub max_results: usize,
    pub throttle: String,
    pub exclude_patterns: Vec<String>,
}

impl Default for CliConfig {
    fn default() -> Self {
        Self {
            max_results: 20,
            throttle: "off".to_string(),
            exclude_patterns: vec!["*.tmp".to_string()],
        }
    }
}

impl CliConfig {
    /// 從 .recoll-next/config.toml 載入設定，找不到則回傳預設值
    pub fn load_from(base_path: &Path) -> Result<Self> {
        let config_path = base_path.join(".recoll-next").join("config.toml");
        if config_path.exists() {
            let content = std::fs::read_to_string(config_path)?;
            let cfg: CliConfig = toml::from_str(&content)?;
            Ok(cfg)
        } else {
            Ok(Self::default())
        }
    }
}
