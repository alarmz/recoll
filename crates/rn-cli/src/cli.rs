use clap::{Parser, Subcommand, ValueEnum};
use std::path::PathBuf;

/// I/O 節流模式
#[derive(Debug, Clone, Copy, PartialEq, Eq, ValueEnum)]
pub enum ThrottleArg {
    Off,
    Gentle,
    Aggressive,
}

#[derive(Parser, Debug)]
#[command(name = "rn", about = "Recoll Next — desktop full-text search")]
pub struct Cli {
    /// 自訂設定檔路徑
    #[arg(long, global = true)]
    pub config: Option<PathBuf>,

    #[command(subcommand)]
    pub command: Command,
}

#[derive(Subcommand, Debug)]
pub enum Command {
    /// 初始化索引目錄
    Init {
        /// 要索引的根目錄
        path: PathBuf,
        /// 強制覆蓋既有設定
        #[arg(long, default_value_t = false)]
        force: bool,
    },
    /// 搜尋
    Search {
        /// 搜尋關鍵字
        query: String,
        /// 最大結果數
        #[arg(long, default_value_t = 20)]
        limit: usize,
        /// 偏移量
        #[arg(long, default_value_t = 0)]
        offset: usize,
        /// 輸出 JSON 格式
        #[arg(long, default_value_t = false)]
        json: bool,
        /// 不顯示 snippet
        #[arg(long, default_value_t = false)]
        no_snippet: bool,
        /// 檔案類型過濾
        #[arg(long = "type")]
        file_type: Option<String>,
    },
    /// 執行索引
    Index {
        /// 要索引的根目錄
        path: PathBuf,
        /// 全量重建索引
        #[arg(long, default_value_t = false)]
        full: bool,
        /// 模擬執行
        #[arg(long, default_value_t = false)]
        dry_run: bool,
        /// I/O 節流模式
        #[arg(long, value_enum, default_value_t = ThrottleArg::Off)]
        throttle: ThrottleArg,
    },
    /// 統計資訊
    Stats {
        /// 輸出 JSON 格式
        #[arg(long, default_value_t = false)]
        json: bool,
    },
    /// 健康檢查
    Doctor {
        /// 自動修復
        #[arg(long, default_value_t = false)]
        fix: bool,
        /// 詳細輸出
        #[arg(long, default_value_t = false)]
        verbose: bool,
    },
}
