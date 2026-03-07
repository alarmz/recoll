use serde::{Deserialize, Serialize};

/// 元件狀態
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum ComponentStatus {
    Ok,
    Degraded(String),
    Down(String),
}

/// 單一元件健康
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ComponentHealth {
    pub name: String,
    pub status: ComponentStatus,
}

/// 整體健康狀態
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum OverallStatus {
    Healthy,
    Degraded,
    Unhealthy,
}

/// 健康報告
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthReport {
    pub status: OverallStatus,
    pub components: Vec<ComponentHealth>,
}

impl HealthReport {
    /// 從元件狀態建立報告
    pub fn from_components(components: Vec<ComponentHealth>) -> Self {
        let status = if components
            .iter()
            .any(|c| matches!(c.status, ComponentStatus::Down(_)))
        {
            OverallStatus::Unhealthy
        } else if components
            .iter()
            .any(|c| matches!(c.status, ComponentStatus::Degraded(_)))
        {
            OverallStatus::Degraded
        } else {
            OverallStatus::Healthy
        };
        Self { status, components }
    }

    /// 整體狀態
    pub fn overall(&self) -> &OverallStatus {
        &self.status
    }
}
