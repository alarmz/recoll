use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum UpgradeDirection {
    Upgrade,
    Downgrade,
    Same,
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub struct VersionInfo {
    pub major: u32,
    pub minor: u32,
    pub patch: u32,
}

impl VersionInfo {
    pub fn new(major: u32, minor: u32, patch: u32) -> Self {
        Self {
            major,
            minor,
            patch,
        }
    }

    pub fn upgrade_from(&self, old: &VersionInfo) -> UpgradeDirection {
        match self.cmp(old) {
            std::cmp::Ordering::Greater => UpgradeDirection::Upgrade,
            std::cmp::Ordering::Less => UpgradeDirection::Downgrade,
            std::cmp::Ordering::Equal => UpgradeDirection::Same,
        }
    }
}

impl std::str::FromStr for VersionInfo {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        let parts: Vec<&str> = s.split('.').collect();
        if parts.len() != 3 {
            return Err(format!("invalid version: {s}"));
        }
        Ok(Self {
            major: parts[0]
                .parse()
                .map_err(|_| format!("invalid major: {}", parts[0]))?,
            minor: parts[1]
                .parse()
                .map_err(|_| format!("invalid minor: {}", parts[1]))?,
            patch: parts[2]
                .parse()
                .map_err(|_| format!("invalid patch: {}", parts[2]))?,
        })
    }
}

impl fmt::Display for VersionInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}.{}", self.major, self.minor, self.patch)
    }
}
