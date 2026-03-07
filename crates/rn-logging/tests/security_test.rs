use rn_logging::security::SecurityPolicy;

#[test]
fn default_binds_localhost_only() {
    let policy = SecurityPolicy::default();
    assert!(policy.bind_localhost_only);
}

#[test]
fn default_max_snippet_len() {
    let policy = SecurityPolicy::default();
    assert_eq!(policy.max_snippet_len, 200);
}

#[test]
fn default_excluded_dirs() {
    let policy = SecurityPolicy::default();
    assert!(policy.excluded_dirs.contains(&".ssh".to_string()));
    assert!(policy.excluded_dirs.contains(&".gnupg".to_string()));
}
