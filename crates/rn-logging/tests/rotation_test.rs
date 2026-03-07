use rn_logging::config::LogConfig;
use rn_logging::rotation::Rotation;

#[test]
fn rotation_display() {
    assert_eq!(format!("{}", Rotation::Daily), "daily");
    assert_eq!(format!("{}", Rotation::Hourly), "hourly");
    assert_eq!(format!("{}", Rotation::Never), "never");
}

#[test]
fn default_max_files() {
    let cfg = LogConfig::default();
    assert_eq!(cfg.max_files, 7);
}

#[test]
fn default_max_size_mb() {
    let cfg = LogConfig::default();
    assert_eq!(cfg.max_size_mb, 50);
}
