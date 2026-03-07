/// 系統匣動作
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub enum TrayAction {
    Show,
    Hide,
    Quit,
}

impl TrayAction {
    /// 所有動作
    pub fn all() -> Vec<TrayAction> {
        vec![TrayAction::Show, TrayAction::Hide, TrayAction::Quit]
    }

    /// 動作標籤
    pub fn label(self) -> &'static str {
        match self {
            TrayAction::Show => "Show Window",
            TrayAction::Hide => "Hide Window",
            TrayAction::Quit => "Quit",
        }
    }

    /// 從 ID 字串解析
    pub fn from_id(id: &str) -> Option<Self> {
        match id {
            "show" => Some(TrayAction::Show),
            "hide" => Some(TrayAction::Hide),
            "quit" => Some(TrayAction::Quit),
            _ => None,
        }
    }
}
