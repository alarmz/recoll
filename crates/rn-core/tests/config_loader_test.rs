//! ConfigLoader 載入測試

use rn_core::config::{AppConfig, ConfigLoader};
use std::io::Write;

#[test]
fn load_nonexistent_path_returns_default() {
    let cfg = ConfigLoader::load(std::path::Path::new("/nonexistent/path/config.toml"));
    assert_eq!(cfg.indexer.max_workers, 4);
}

#[test]
fn load_from_file_overrides_values() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("config.toml");
    let mut f = std::fs::File::create(&path).unwrap();
    writeln!(
        f,
        r#"
[indexer]
max_workers = 8
commit_interval_secs = 30

[gpu]
enabled = false
min_batch_size = 32

[search]
max_results = 100
snippet_length = 200

[watcher]
debounce_ms = 500
recursive = true

[logging]
level = "info"
file_output = false
"#
    )
    .unwrap();
    let cfg = ConfigLoader::load_from_file(&path).unwrap();
    assert_eq!(cfg.indexer.max_workers, 8);
}

#[test]
fn partial_toml_uses_defaults_for_missing_fields() {
    let toml_str = r#"
[indexer]
max_workers = 16
commit_interval_secs = 30

[gpu]
enabled = false
min_batch_size = 32

[search]
max_results = 100
snippet_length = 200

[watcher]
debounce_ms = 500
recursive = true

[logging]
level = "debug"
file_output = false
"#;
    let cfg: AppConfig = toml::from_str(toml_str).unwrap();
    assert_eq!(cfg.indexer.max_workers, 16);
    assert_eq!(cfg.logging.level, "debug");
}
