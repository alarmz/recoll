use rn_installer::release::{ReleaseArtifact, ReleaseInfo};

#[test]
fn create_release_with_artifacts() {
    let artifacts = vec![
        ReleaseArtifact::new("recoll-next.exe", "windows"),
        ReleaseArtifact::new("recoll-next", "linux"),
    ];
    let rel = ReleaseInfo::new("v0.1.0", artifacts);
    assert_eq!(rel.tag, "v0.1.0");
    assert_eq!(rel.artifacts.len(), 2);
}

#[test]
fn filter_artifacts_by_platform() {
    let artifacts = vec![
        ReleaseArtifact::new("recoll-next.exe", "windows"),
        ReleaseArtifact::new("recoll-next.msi", "windows"),
        ReleaseArtifact::new("recoll-next", "linux"),
    ];
    let rel = ReleaseInfo::new("v0.1.0", artifacts);
    let win = rel.artifacts_for("windows");
    assert_eq!(win.len(), 2);
    let linux = rel.artifacts_for("linux");
    assert_eq!(linux.len(), 1);
}

#[test]
fn release_display_format() {
    let artifacts = vec![
        ReleaseArtifact::new("a.exe", "windows"),
        ReleaseArtifact::new("b", "linux"),
    ];
    let rel = ReleaseInfo::new("v0.1.0", artifacts);
    let s = format!("{}", rel);
    assert!(s.contains("v0.1.0"));
    assert!(s.contains("2 artifacts"));
}
