//! Init 命令測試

use rn_cli::commands::init::run_init;

#[test]
fn creates_index_directory() {
    let dir = tempfile::tempdir().unwrap();
    run_init(dir.path()).unwrap();
    assert!(dir.path().join(".recoll-next").is_dir());
}

#[test]
fn creates_default_config_file() {
    let dir = tempfile::tempdir().unwrap();
    run_init(dir.path()).unwrap();
    let config_path = dir.path().join(".recoll-next").join("config.toml");
    assert!(config_path.is_file());
    let content = std::fs::read_to_string(config_path).unwrap();
    assert!(content.contains("max_results"));
}

#[test]
fn duplicate_init_returns_error() {
    let dir = tempfile::tempdir().unwrap();
    run_init(dir.path()).unwrap();
    let result = run_init(dir.path());
    assert!(result.is_err());
}
