use rn_bench::bench_config::{BenchConfig, OutputFormat};

#[test]
fn default_iterations() {
    let cfg = BenchConfig::default();
    assert_eq!(cfg.iterations, 100);
}

#[test]
fn default_warmup_secs() {
    let cfg = BenchConfig::default();
    assert_eq!(cfg.warmup_secs, 3);
}

#[test]
fn default_output_format_is_json() {
    let cfg = BenchConfig::default();
    assert_eq!(cfg.output_format, OutputFormat::Json);
}
