use rn_logging::config::{LogConfig, LogLevel};
use rn_logging::rotation::Rotation;

#[test]
fn default_log_level_is_info() {
    let cfg = LogConfig::default();
    assert_eq!(cfg.level, LogLevel::Info);
}

#[test]
fn default_rotation_is_daily() {
    let cfg = LogConfig::default();
    assert_eq!(cfg.rotation, Rotation::Daily);
}

#[test]
fn log_level_parse() {
    assert_eq!(LogLevel::parse("debug"), Some(LogLevel::Debug));
    assert_eq!(LogLevel::parse("info"), Some(LogLevel::Info));
    assert_eq!(LogLevel::parse("warn"), Some(LogLevel::Warn));
    assert_eq!(LogLevel::parse("error"), Some(LogLevel::Error));
    assert_eq!(LogLevel::parse("unknown"), None);
}
