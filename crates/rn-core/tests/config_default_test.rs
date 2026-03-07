//! AppConfig 預設值與序列化測試

use rn_core::config::AppConfig;

#[test]
fn default_config_has_reasonable_values() {
    let cfg = AppConfig::default();
    assert_eq!(cfg.indexer.max_workers, 4);
    assert_eq!(cfg.indexer.commit_interval_secs, 30);
    assert!(!cfg.gpu.enabled);
    assert_eq!(cfg.logging.level, "info");
}

#[test]
fn config_serializes_to_toml_with_sections() {
    let cfg = AppConfig::default();
    let toml_str = toml::to_string(&cfg).unwrap();
    assert!(toml_str.contains("[indexer]"));
    assert!(toml_str.contains("[search]"));
    assert!(toml_str.contains("[gpu]"));
}

#[test]
fn config_roundtrip_toml() {
    let cfg = AppConfig::default();
    let toml_str = toml::to_string(&cfg).unwrap();
    let parsed: AppConfig = toml::from_str(&toml_str).unwrap();
    assert_eq!(parsed.indexer.max_workers, cfg.indexer.max_workers);
    assert_eq!(parsed.gpu.enabled, cfg.gpu.enabled);
    assert_eq!(parsed.search.max_results, cfg.search.max_results);
}
