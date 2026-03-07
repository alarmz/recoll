use rn_bench::stress::StressConfig;

#[test]
fn default_file_count() {
    let cfg = StressConfig::default();
    assert_eq!(cfg.file_count, 100_000);
}

#[test]
fn default_format_mix() {
    let cfg = StressConfig::default();
    assert!(cfg.format_mix.contains(&"text".to_string()));
    assert!(cfg.format_mix.contains(&"html".to_string()));
    assert!(cfg.format_mix.contains(&"pdf".to_string()));
}

#[test]
fn stress_display_format() {
    let cfg = StressConfig::default();
    let s = format!("{}", cfg);
    assert!(s.contains("100000 files"));
    assert!(s.contains("formats"));
}
