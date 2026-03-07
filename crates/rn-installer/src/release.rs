use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Platform {
    Windows,
    Linux,
    MacOs,
}

impl Platform {
    pub fn parse(s: &str) -> Option<Self> {
        match s {
            "windows" => Some(Platform::Windows),
            "linux" => Some(Platform::Linux),
            "macos" => Some(Platform::MacOs),
            _ => None,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReleaseArtifact {
    pub filename: String,
    pub platform: Platform,
}

impl ReleaseArtifact {
    pub fn new(filename: &str, platform: &str) -> Self {
        Self {
            filename: filename.to_string(),
            platform: Platform::parse(platform).unwrap_or(Platform::Windows),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ReleaseInfo {
    pub tag: String,
    pub artifacts: Vec<ReleaseArtifact>,
}

impl ReleaseInfo {
    pub fn new(tag: &str, artifacts: Vec<ReleaseArtifact>) -> Self {
        Self {
            tag: tag.to_string(),
            artifacts,
        }
    }

    pub fn artifacts_for(&self, platform: &str) -> Vec<&ReleaseArtifact> {
        let p = Platform::parse(platform);
        self.artifacts
            .iter()
            .filter(|a| Some(a.platform) == p)
            .collect()
    }
}

impl fmt::Display for ReleaseInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "Release {} ({} artifacts)",
            self.tag,
            self.artifacts.len()
        )
    }
}
