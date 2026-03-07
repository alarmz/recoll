use rn_installer::inno::{Compression, InnoConfig};

#[test]
fn default_inno_values() {
    let cfg = InnoConfig::default();
    assert_eq!(cfg.app_name(), "Recoll Next");
    assert_eq!(cfg.version(), "0.1.0");
}

#[test]
fn inno_output_base_filename() {
    let cfg = InnoConfig::default();
    assert_eq!(cfg.output_base_filename(), "recoll-next-0.1.0-win64-setup");
}

#[test]
fn inno_default_compression_is_lzma2() {
    let cfg = InnoConfig::default();
    assert_eq!(cfg.compression, Compression::Lzma2);
}
