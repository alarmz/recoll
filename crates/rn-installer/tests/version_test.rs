use rn_installer::version::{UpgradeDirection, VersionInfo};

#[test]
fn parse_semantic_version() {
    let v: VersionInfo = "1.2.3".parse().unwrap();
    assert_eq!(v.major, 1);
    assert_eq!(v.minor, 2);
    assert_eq!(v.patch, 3);
}

#[test]
fn upgrade_vs_downgrade() {
    let v1 = VersionInfo::new(1, 0, 0);
    let v2 = VersionInfo::new(2, 0, 0);
    assert_eq!(v2.upgrade_from(&v1), UpgradeDirection::Upgrade);
    assert_eq!(v1.upgrade_from(&v2), UpgradeDirection::Downgrade);
    assert_eq!(v1.upgrade_from(&v1), UpgradeDirection::Same);
}

#[test]
fn version_display_format() {
    let v = VersionInfo::new(1, 2, 3);
    assert_eq!(format!("{}", v), "1.2.3");
}
