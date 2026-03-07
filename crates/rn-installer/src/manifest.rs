use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Component {
    Binary,
    Service,
    ShellExtension,
    Shortcut,
    PathEntry,
}

impl Component {
    pub fn is_essential(self) -> bool {
        matches!(self, Component::Binary | Component::PathEntry)
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct InstallerManifest {
    pub components: Vec<Component>,
}

impl Default for InstallerManifest {
    fn default() -> Self {
        Self {
            components: vec![
                Component::Binary,
                Component::Service,
                Component::ShellExtension,
                Component::Shortcut,
                Component::PathEntry,
            ],
        }
    }
}

impl InstallerManifest {
    pub fn essential_only(&self) -> Self {
        Self {
            components: self
                .components
                .iter()
                .copied()
                .filter(|c| c.is_essential())
                .collect(),
        }
    }
}

impl fmt::Display for InstallerManifest {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Install manifest ({} components)", self.components.len())
    }
}
