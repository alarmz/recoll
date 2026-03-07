use rn_installer::manifest::{Component, InstallerManifest};

#[test]
fn default_manifest_has_all_components() {
    let m = InstallerManifest::default();
    assert_eq!(m.components.len(), 5);
    assert!(m.components.contains(&Component::Binary));
    assert!(m.components.contains(&Component::Service));
    assert!(m.components.contains(&Component::ShellExtension));
    assert!(m.components.contains(&Component::Shortcut));
    assert!(m.components.contains(&Component::PathEntry));
}

#[test]
fn essential_only_filters_components() {
    let m = InstallerManifest::default();
    let essential = m.essential_only();
    assert_eq!(essential.components.len(), 2);
    assert!(essential.components.contains(&Component::Binary));
    assert!(essential.components.contains(&Component::PathEntry));
}

#[test]
fn manifest_display_shows_count() {
    let m = InstallerManifest::default();
    let s = format!("{}", m);
    assert!(s.contains("5 components"));
}
