use std::fmt;

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum KpiStatus {
    Pass,
    Fail,
}

impl fmt::Display for KpiStatus {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            KpiStatus::Pass => write!(f, "PASS"),
            KpiStatus::Fail => write!(f, "FAIL"),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KpiResult {
    pub name: String,
    pub actual: u64,
    pub threshold: u64,
    pub status: KpiStatus,
    pub message: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KpiThresholds {
    pub filename_search_ms: u64,
    pub fulltext_search_ms: u64,
    pub index_throughput_fps: u64,
}

impl Default for KpiThresholds {
    fn default() -> Self {
        Self {
            filename_search_ms: 30,
            fulltext_search_ms: 300,
            index_throughput_fps: 500,
        }
    }
}

impl KpiThresholds {
    pub fn check(&self, name: &str, actual: u64) -> KpiResult {
        let threshold = match name {
            "filename_search" => self.filename_search_ms,
            "fulltext_search" => self.fulltext_search_ms,
            "index_throughput" => self.index_throughput_fps,
            _ => 0,
        };

        let is_throughput = name == "index_throughput";
        let passed = if is_throughput {
            actual >= threshold
        } else {
            actual <= threshold
        };

        let status = if passed {
            KpiStatus::Pass
        } else {
            KpiStatus::Fail
        };

        let message = if passed {
            format!("{name}: {actual} (threshold: {threshold}) OK")
        } else {
            format!("{name}: {actual} exceeded threshold {threshold}")
        };

        KpiResult {
            name: name.to_string(),
            actual,
            threshold,
            status,
            message,
        }
    }
}
