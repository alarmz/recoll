use rn_bench::resource::ResourceLimits;

#[test]
fn default_max_memory_mb() {
    let limits = ResourceLimits::default();
    assert_eq!(limits.max_memory_mb, 512);
}

#[test]
fn battery_mode_is_constrained() {
    let limits = ResourceLimits {
        battery_mode: true,
        ..ResourceLimits::default()
    };
    assert!(limits.is_constrained());
}

#[test]
fn default_is_not_constrained() {
    let limits = ResourceLimits::default();
    assert!(!limits.is_constrained());
}
