//! HealthReport 健康檢查測試

use rn_sdk::health::{ComponentHealth, ComponentStatus, HealthReport, OverallStatus};

#[test]
fn all_ok_returns_healthy() {
    let components = vec![
        ComponentHealth {
            name: "db".to_string(),
            status: ComponentStatus::Ok,
        },
        ComponentHealth {
            name: "index".to_string(),
            status: ComponentStatus::Ok,
        },
    ];
    let report = HealthReport::from_components(components);
    assert_eq!(*report.overall(), OverallStatus::Healthy);
}

#[test]
fn any_degraded_returns_degraded() {
    let components = vec![
        ComponentHealth {
            name: "db".to_string(),
            status: ComponentStatus::Ok,
        },
        ComponentHealth {
            name: "gpu".to_string(),
            status: ComponentStatus::Degraded("slow".to_string()),
        },
    ];
    let report = HealthReport::from_components(components);
    assert_eq!(*report.overall(), OverallStatus::Degraded);
}

#[test]
fn health_report_serializes_to_json_with_status() {
    let components = vec![ComponentHealth {
        name: "db".to_string(),
        status: ComponentStatus::Ok,
    }];
    let report = HealthReport::from_components(components);
    let json = serde_json::to_string(&report).unwrap();
    assert!(json.contains("\"status\""));
}
