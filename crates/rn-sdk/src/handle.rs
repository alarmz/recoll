use std::path::PathBuf;

/// SDK 設定
#[derive(Debug, Clone, PartialEq, serde::Serialize, serde::Deserialize)]
pub struct SdkConfig {
    pub bind_address: String,
    pub port: u16,
    pub data_dir: PathBuf,
}

impl Default for SdkConfig {
    fn default() -> Self {
        Self {
            bind_address: "127.0.0.1".to_string(),
            port: 9312,
            data_dir: PathBuf::from(".recoll-next"),
        }
    }
}

/// SDK Handle
pub struct RecollNext {
    open: bool,
    config: SdkConfig,
}

impl RecollNext {
    /// 開啟 SDK
    pub fn open(config: SdkConfig) -> anyhow::Result<Self> {
        Ok(Self { open: true, config })
    }

    /// 取得設定
    pub fn config(&self) -> &SdkConfig {
        &self.config
    }

    /// 是否開啟
    pub fn is_open(&self) -> bool {
        self.open
    }

    /// 關閉 SDK
    pub fn close(&mut self) {
        self.open = false;
    }
}
