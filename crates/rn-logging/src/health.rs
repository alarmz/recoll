use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum CheckStatus {
    Healthy,
    Warning(String),
    Critical(String),
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OverallHealth {
    Healthy,
    Degraded,
    Unhealthy,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CheckResult {
    pub component: String,
    pub status: CheckStatus,
}

impl CheckResult {
    pub fn new(component: &str, status: CheckStatus) -> Self {
        Self {
            component: component.to_string(),
            status,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthCheck {
    pub results: Vec<CheckResult>,
    overall: OverallHealth,
}

impl HealthCheck {
    pub fn evaluate(results: Vec<CheckResult>) -> Self {
        let overall = if results
            .iter()
            .any(|r| matches!(r.status, CheckStatus::Critical(_)))
        {
            OverallHealth::Unhealthy
        } else if results
            .iter()
            .any(|r| matches!(r.status, CheckStatus::Warning(_)))
        {
            OverallHealth::Degraded
        } else {
            OverallHealth::Healthy
        };
        Self { results, overall }
    }

    pub fn overall(&self) -> OverallHealth {
        self.overall
    }
}
