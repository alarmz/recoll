use anyhow::Result;
use std::path::{Path, PathBuf};
use walkdir::WalkDir;

/// 檔案爬取器
pub struct Crawler {
    root: PathBuf,
    exclude_patterns: Vec<glob::Pattern>,
}

impl Crawler {
    pub fn new(root: &Path, exclude_patterns: &[&str]) -> Self {
        let patterns = exclude_patterns
            .iter()
            .filter_map(|p| glob::Pattern::new(p).ok())
            .collect();
        Self {
            root: root.to_path_buf(),
            exclude_patterns: patterns,
        }
    }

    pub fn scan(&self) -> Result<Vec<PathBuf>> {
        let mut files = Vec::new();
        for entry in WalkDir::new(&self.root)
            .into_iter()
            .filter_entry(|e| !is_hidden(e))
        {
            let entry = entry?;
            if !entry.file_type().is_file() {
                continue;
            }
            let path = entry.path();
            if self.is_excluded(path) {
                continue;
            }
            files.push(path.to_path_buf());
        }
        Ok(files)
    }

    fn is_excluded(&self, path: &Path) -> bool {
        let filename = path.file_name().and_then(|n| n.to_str()).unwrap_or("");
        self.exclude_patterns.iter().any(|p| p.matches(filename))
    }
}

fn is_hidden(entry: &walkdir::DirEntry) -> bool {
    if entry.depth() == 0 {
        return false;
    }
    entry
        .file_name()
        .to_str()
        .map(|s| s.starts_with('.'))
        .unwrap_or(false)
}
