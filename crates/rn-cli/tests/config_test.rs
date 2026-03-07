//! Config 測試

use rn_cli::config::CliConfig;

#[test]
fn default_config_has_expected_fields() {
    let cfg = CliConfig::default();
    assert!(cfg.max_results > 0);
    assert_eq!(cfg.throttle, "off");
}

#[test]
fn config_serializes_to_toml() {
    let cfg = CliConfig::default();
    let toml_str = toml::to_string(&cfg).unwrap();
    assert!(toml_str.contains("max_results"));
    assert!(toml_str.contains("throttle"));
}

#[test]
fn config_deserializes_from_toml() {
    let toml_str = r#"
max_results = 50
throttle = "gentle"
exclude_patterns = ["*.log", "*.tmp"]
"#;
    let cfg: CliConfig = toml::from_str(toml_str).unwrap();
    assert_eq!(cfg.max_results, 50);
    assert_eq!(cfg.throttle, "gentle");
    assert_eq!(cfg.exclude_patterns, vec!["*.log", "*.tmp"]);
}
