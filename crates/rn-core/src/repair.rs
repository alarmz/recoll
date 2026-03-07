use std::fmt;

/// 診斷問題類型
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Issue {
    StaleLock,
    OrphanState,
    CorruptedIndex,
}

/// 修復動作
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RepairAction {
    RemoveLock,
    ResetState,
    RebuildIndex,
}

impl fmt::Display for RepairAction {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            RepairAction::RemoveLock => write!(f, "Remove stale lock file"),
            RepairAction::ResetState => write!(f, "Reset orphan document states"),
            RepairAction::RebuildIndex => write!(f, "Rebuild corrupted index"),
        }
    }
}

/// 修復計畫
#[derive(Debug, Clone)]
pub struct RepairPlan {
    pub actions: Vec<RepairAction>,
}

impl RepairPlan {
    /// 根據問題清單產生修復計畫
    pub fn diagnose(issues: &[Issue]) -> Self {
        let actions = issues
            .iter()
            .map(|issue| match issue {
                Issue::StaleLock => RepairAction::RemoveLock,
                Issue::OrphanState => RepairAction::ResetState,
                Issue::CorruptedIndex => RepairAction::RebuildIndex,
            })
            .collect();
        Self { actions }
    }
}

impl fmt::Display for RepairPlan {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if self.actions.is_empty() {
            write!(f, "No issues found.")
        } else {
            writeln!(f, "Repair plan ({} actions):", self.actions.len())?;
            for (i, action) in self.actions.iter().enumerate() {
                writeln!(f, "  {}. {action}", i + 1)?;
            }
            Ok(())
        }
    }
}
