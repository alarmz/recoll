use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Rotation {
    Daily,
    Hourly,
    Never,
}

impl fmt::Display for Rotation {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Rotation::Daily => write!(f, "daily"),
            Rotation::Hourly => write!(f, "hourly"),
            Rotation::Never => write!(f, "never"),
        }
    }
}
