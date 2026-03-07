use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ResourceLimits {
    pub max_memory_mb: u64,
    pub battery_mode: bool,
    pub cpu_limit_percent: u8,
}

impl Default for ResourceLimits {
    fn default() -> Self {
        Self {
            max_memory_mb: 512,
            battery_mode: false,
            cpu_limit_percent: 100,
        }
    }
}

impl ResourceLimits {
    pub fn constrained() -> Self {
        Self {
            max_memory_mb: 256,
            battery_mode: true,
            cpu_limit_percent: 50,
        }
    }

    pub fn is_constrained(&self) -> bool {
        self.battery_mode || self.cpu_limit_percent < 100 || self.max_memory_mb < 256
    }
}
