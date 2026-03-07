use anyhow::{bail, Result};
use std::fs;
use std::path::Path;

use crate::config::CliConfig;

/// 執行 init 命令：建立 .recoll-next 目錄與預設設定檔
pub fn run_init(path: &Path) -> Result<()> {
    run_init_with_force(path, false)
}

/// 執行 init 命令，支援 force 覆蓋
pub fn run_init_with_force(path: &Path, force: bool) -> Result<()> {
    let index_dir = path.join(".recoll-next");
    if index_dir.exists() && !force {
        bail!("already initialized: {}", index_dir.display());
    }
    fs::create_dir_all(&index_dir)?;

    let config = CliConfig::default();
    let toml_str = toml::to_string_pretty(&config)?;
    fs::write(index_dir.join("config.toml"), toml_str)?;

    Ok(())
}
