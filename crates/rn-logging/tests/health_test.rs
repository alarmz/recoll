use rn_logging::health::{CheckResult, CheckStatus, HealthCheck, OverallHealth};

#[test]
fn all_healthy() {
    let checks = vec![
        CheckResult::new("db", CheckStatus::Healthy),
        CheckResult::new("search", CheckStatus::Healthy),
    ];
    let hc = HealthCheck::evaluate(checks);
    assert_eq!(hc.overall(), OverallHealth::Healthy);
}

#[test]
fn one_warning_means_degraded() {
    let checks = vec![
        CheckResult::new("db", CheckStatus::Healthy),
        CheckResult::new("search", CheckStatus::Warning("slow".into())),
    ];
    let hc = HealthCheck::evaluate(checks);
    assert_eq!(hc.overall(), OverallHealth::Degraded);
}

#[test]
fn one_critical_means_unhealthy() {
    let checks = vec![
        CheckResult::new("db", CheckStatus::Critical("down".into())),
        CheckResult::new("search", CheckStatus::Healthy),
    ];
    let hc = HealthCheck::evaluate(checks);
    assert_eq!(hc.overall(), OverallHealth::Unhealthy);
}
