use std::path::PathBuf;

/// 在臨時目錄中建立測試檔案，回傳路徑
pub fn write_temp_file(dir: &tempfile::TempDir, name: &str, content: &[u8]) -> PathBuf {
    let path = dir.path().join(name);
    std::fs::write(&path, content).unwrap();
    path
}
