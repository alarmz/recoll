use rn_bench::kpi::{KpiStatus, KpiThresholds};

#[test]
fn default_kpi_thresholds() {
    let kpi = KpiThresholds::default();
    assert_eq!(kpi.filename_search_ms, 30);
    assert_eq!(kpi.fulltext_search_ms, 300);
    assert_eq!(kpi.index_throughput_fps, 500);
}

#[test]
fn kpi_check_pass() {
    let kpi = KpiThresholds::default();
    let result = kpi.check("filename_search", 20);
    assert_eq!(result.status, KpiStatus::Pass);
}

#[test]
fn kpi_check_fail() {
    let kpi = KpiThresholds::default();
    let result = kpi.check("filename_search", 50);
    assert_eq!(result.status, KpiStatus::Fail);
    assert!(result.message.contains("50"));
}
