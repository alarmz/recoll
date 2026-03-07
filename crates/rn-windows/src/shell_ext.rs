use std::path::Path;

/// Windows Explorer Shell Extension 設定
pub struct ShellExtension {
    app_name: String,
    exe_path: String,
}

impl ShellExtension {
    pub fn new(exe_path: &Path) -> Self {
        Self {
            app_name: "RecollNext".to_string(),
            exe_path: exe_path.to_string_lossy().to_string(),
        }
    }

    /// Registry 路徑
    pub fn registry_path(&self) -> String {
        format!(r"Directory\Background\shell\{}", self.app_name)
    }

    /// 右鍵選單文字
    pub fn menu_text(&self) -> &str {
        "Search with Recoll Next"
    }

    /// 命令列（含 %V 佔位符）
    pub fn command_line(&self) -> String {
        format!(r#""{}" search --path "%V""#, self.exe_path)
    }
}
