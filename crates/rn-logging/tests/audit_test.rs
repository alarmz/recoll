use rn_logging::audit::AuditEntry;

#[test]
fn create_audit_entry() {
    let entry = AuditEntry::new("search", "admin");
    assert_eq!(entry.action, "search");
    assert_eq!(entry.user, "admin");
}

#[test]
fn sanitize_masks_password() {
    let entry = AuditEntry::new("login", "admin").with_detail("password=secret123");
    let sanitized = entry.sanitize();
    assert!(!sanitized.detail.as_ref().unwrap().contains("secret123"));
    assert!(sanitized.detail.as_ref().unwrap().contains("***"));
}

#[test]
fn audit_display_format() {
    let entry = AuditEntry::new("search", "admin");
    let s = format!("{}", entry);
    assert!(s.contains("[search]"));
    assert!(s.contains("by admin"));
}
