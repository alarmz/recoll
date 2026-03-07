use std::fmt;

use serde::{Deserialize, Serialize};

use crate::kpi::KpiStatus;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BenchmarkResult {
    pub name: String,
    pub value: u64,
    pub status: KpiStatus,
}

impl BenchmarkResult {
    pub fn new(name: &str, value: u64, status: KpiStatus) -> Self {
        Self {
            name: name.to_string(),
            value,
            status,
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BenchmarkReport {
    pub results: Vec<BenchmarkResult>,
}

impl BenchmarkReport {
    pub fn new(results: Vec<BenchmarkResult>) -> Self {
        Self { results }
    }

    pub fn passed(&self) -> usize {
        self.results
            .iter()
            .filter(|r| r.status == KpiStatus::Pass)
            .count()
    }

    pub fn failed(&self) -> usize {
        self.results
            .iter()
            .filter(|r| r.status == KpiStatus::Fail)
            .count()
    }

    pub fn summary(&self) -> String {
        let failed = self.failed();
        if failed == 0 {
            format!("All {} benchmarks passed", self.results.len())
        } else {
            format!("{} of {} benchmarks failed", failed, self.results.len())
        }
    }
}

impl fmt::Display for BenchmarkReport {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Benchmark report: {} results", self.results.len())
    }
}
