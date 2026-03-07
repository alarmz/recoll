use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AuditEntry {
    pub action: String,
    pub user: String,
    pub detail: Option<String>,
    pub created_at: String,
}

impl AuditEntry {
    pub fn new(action: &str, user: &str) -> Self {
        Self {
            action: action.to_string(),
            user: user.to_string(),
            detail: None,
            created_at: String::new(),
        }
    }

    pub fn with_detail(mut self, detail: &str) -> Self {
        self.detail = Some(detail.to_string());
        self
    }

    pub fn sanitize(&self) -> Self {
        let detail = self.detail.as_ref().map(|d| {
            let mut sanitized = d.clone();
            for keyword in &["password", "secret", "token", "key"] {
                if let Some(pos) = sanitized.to_lowercase().find(keyword) {
                    if let Some(eq_pos) = sanitized[pos..].find('=') {
                        let value_start = pos + eq_pos + 1;
                        let value_end = sanitized[value_start..]
                            .find(|c: char| c.is_whitespace() || c == '&' || c == ';')
                            .map(|p| value_start + p)
                            .unwrap_or(sanitized.len());
                        sanitized.replace_range(value_start..value_end, "***");
                    }
                }
            }
            sanitized
        });
        Self {
            action: self.action.clone(),
            user: self.user.clone(),
            detail,
            created_at: self.created_at.clone(),
        }
    }
}

impl fmt::Display for AuditEntry {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[{}] by {}", self.action, self.user)
    }
}
