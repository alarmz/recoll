use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SecurityPolicy {
    pub bind_localhost_only: bool,
    pub max_snippet_len: usize,
    pub excluded_dirs: Vec<String>,
}

impl SecurityPolicy {
    pub fn is_excluded(&self, path: &str) -> bool {
        self.excluded_dirs
            .iter()
            .any(|dir| path.contains(dir.as_str()))
    }
}

impl Default for SecurityPolicy {
    fn default() -> Self {
        Self {
            bind_localhost_only: true,
            max_snippet_len: 200,
            excluded_dirs: vec![
                ".ssh".to_string(),
                ".gnupg".to_string(),
                ".aws".to_string(),
                ".credentials".to_string(),
            ],
        }
    }
}
