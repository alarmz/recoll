use serde::{Deserialize, Serialize};
use std::path::Path;

/// 索引器設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct IndexerConfig {
    pub max_workers: usize,
    pub commit_interval_secs: u64,
}

/// GPU 設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct GpuConfig {
    pub enabled: bool,
    pub min_batch_size: usize,
}

/// 搜尋設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct SearchConfig {
    pub max_results: usize,
    pub snippet_length: usize,
}

/// 監控設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct WatcherConfig {
    pub debounce_ms: u64,
    pub recursive: bool,
}

/// 日誌設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub level: String,
    pub file_output: bool,
}

/// 應用程式總設定
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub struct AppConfig {
    pub indexer: IndexerConfig,
    pub gpu: GpuConfig,
    pub search: SearchConfig,
    pub watcher: WatcherConfig,
    pub logging: LoggingConfig,
}

impl Default for AppConfig {
    fn default() -> Self {
        Self {
            indexer: IndexerConfig {
                max_workers: 4,
                commit_interval_secs: 30,
            },
            gpu: GpuConfig {
                enabled: false,
                min_batch_size: 32,
            },
            search: SearchConfig {
                max_results: 100,
                snippet_length: 200,
            },
            watcher: WatcherConfig {
                debounce_ms: 500,
                recursive: true,
            },
            logging: LoggingConfig {
                level: "info".to_string(),
                file_output: false,
            },
        }
    }
}

/// 可熱更新欄位
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConfigField {
    LogLevel,
    SnippetLength,
    MaxResults,
    DebounceMs,
    MaxWorkers,
    CommitIntervalSecs,
    GpuEnabled,
}

impl ConfigField {
    /// 是否可熱更新
    pub fn is_hot_reloadable(self) -> bool {
        matches!(
            self,
            ConfigField::LogLevel
                | ConfigField::SnippetLength
                | ConfigField::MaxResults
                | ConfigField::DebounceMs
        )
    }

    /// 從字串解析
    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "log_level" => Some(ConfigField::LogLevel),
            "snippet_length" => Some(ConfigField::SnippetLength),
            "max_results" => Some(ConfigField::MaxResults),
            "debounce_ms" => Some(ConfigField::DebounceMs),
            "max_workers" => Some(ConfigField::MaxWorkers),
            "commit_interval_secs" => Some(ConfigField::CommitIntervalSecs),
            "gpu_enabled" => Some(ConfigField::GpuEnabled),
            _ => None,
        }
    }
}

/// 字串版便利函式（向後相容）
pub fn is_hot_reloadable(field: &str) -> bool {
    ConfigField::parse(field).is_some_and(|f| f.is_hot_reloadable())
}

/// 設定載入器
pub struct ConfigLoader;

impl ConfigLoader {
    /// 從指定路徑載入設定，檔案不存在則回傳預設值
    pub fn load(path: &Path) -> AppConfig {
        if path.exists() {
            Self::load_from_file(path).unwrap_or_default()
        } else {
            AppConfig::default()
        }
    }

    /// 從檔案載入設定
    pub fn load_from_file(path: &Path) -> anyhow::Result<AppConfig> {
        let content = std::fs::read_to_string(path)?;
        let cfg = toml::from_str(&content)?;
        Ok(cfg)
    }
}
