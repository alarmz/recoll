use rn_bench::kpi::KpiStatus;
use rn_bench::report::{BenchmarkReport, BenchmarkResult};

#[test]
fn create_report() {
    let results = vec![
        BenchmarkResult::new("filename_search", 20, KpiStatus::Pass),
        BenchmarkResult::new("fulltext_search", 250, KpiStatus::Pass),
    ];
    let report = BenchmarkReport::new(results);
    assert_eq!(report.results.len(), 2);
}

#[test]
fn all_pass_summary() {
    let results = vec![
        BenchmarkResult::new("a", 10, KpiStatus::Pass),
        BenchmarkResult::new("b", 20, KpiStatus::Pass),
    ];
    let report = BenchmarkReport::new(results);
    assert_eq!(report.summary(), "All 2 benchmarks passed");
}

#[test]
fn report_display_format() {
    let results = vec![BenchmarkResult::new("a", 10, KpiStatus::Pass)];
    let report = BenchmarkReport::new(results);
    let s = format!("{}", report);
    assert!(s.contains("Benchmark report"));
    assert!(s.contains("1 results"));
}
