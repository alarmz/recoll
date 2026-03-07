use std::path::{Path, PathBuf};

/// Lock file 管理
pub struct LockFile {
    path: PathBuf,
}

impl LockFile {
    /// 取得 lock（建立 lock 檔案）
    pub fn acquire(dir: &Path) -> Result<Self, String> {
        let path = Self::lock_path(dir);
        if path.exists() {
            return Err("Lock file already exists".to_string());
        }
        let pid = std::process::id();
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();
        std::fs::write(&path, format!("{pid}\n{now}")).map_err(|e| e.to_string())?;
        Ok(Self { path })
    }

    /// 釋放 lock（刪除 lock 檔案）
    pub fn release(self) -> Result<(), String> {
        let path = self.path.clone();
        std::mem::forget(self);
        std::fs::remove_file(&path).map_err(|e| e.to_string())
    }

    /// 檢查 lock 是否過期（超過 1 小時視為過期）
    pub fn is_stale(dir: &Path) -> bool {
        let path = Self::lock_path(dir);
        let Ok(content) = std::fs::read_to_string(&path) else {
            return false;
        };
        let parts: Vec<&str> = content.trim().split('\n').collect();
        if parts.len() < 2 {
            return true;
        }
        let Ok(timestamp) = parts[1].parse::<u64>() else {
            return true;
        };
        let now = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();
        now.saturating_sub(timestamp) > 3600
    }

    /// lock 檔案路徑
    pub fn lock_path(dir: &Path) -> PathBuf {
        dir.join("recoll-next.lock")
    }
}

impl Drop for LockFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}
