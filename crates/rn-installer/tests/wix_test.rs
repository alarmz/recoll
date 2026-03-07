use rn_installer::wix::WixConfig;

#[test]
fn default_wix_product_info() {
    let cfg = WixConfig::default();
    assert_eq!(cfg.product_name(), "Recoll Next");
    assert_eq!(cfg.manufacturer, "Recoll Project");
}

#[test]
fn wix_install_dir() {
    let cfg = WixConfig::default();
    assert_eq!(cfg.install_dir(), r"C:\Program Files\Recoll Next");
}

#[test]
fn wix_custom_upgrade_code() {
    let code = "custom-uuid-1234";
    let cfg = WixConfig {
        upgrade_code: code.to_string(),
        ..WixConfig::default()
    };
    assert_eq!(cfg.upgrade_code, code);
}
