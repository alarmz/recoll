//! Crawler 行為測試

use rn_indexer::crawler::Crawler;
use std::fs;

#[test]
fn scan_finds_all_files_in_directory() {
    let dir = tempfile::tempdir().unwrap();
    fs::write(dir.path().join("a.txt"), "a").unwrap();
    fs::write(dir.path().join("b.txt"), "b").unwrap();
    fs::write(dir.path().join("c.rs"), "c").unwrap();

    let crawler = Crawler::new(dir.path(), &[]);
    let files = crawler.scan().unwrap();
    assert_eq!(files.len(), 3);
}

#[test]
fn exclude_pattern_filters_matching_files() {
    let dir = tempfile::tempdir().unwrap();
    fs::write(dir.path().join("app.rs"), "code").unwrap();
    fs::write(dir.path().join("debug.log"), "log").unwrap();
    fs::write(dir.path().join("error.log"), "log").unwrap();

    let crawler = Crawler::new(dir.path(), &["*.log"]);
    let files = crawler.scan().unwrap();
    assert_eq!(files.len(), 1);
    assert!(files[0].to_str().unwrap().contains("app.rs"));
}

#[test]
fn scan_recurses_into_subdirectories() {
    let dir = tempfile::tempdir().unwrap();
    let sub = dir.path().join("sub");
    fs::create_dir(&sub).unwrap();
    fs::write(dir.path().join("root.txt"), "r").unwrap();
    fs::write(sub.join("child.txt"), "c").unwrap();

    let crawler = Crawler::new(dir.path(), &[]);
    let files = crawler.scan().unwrap();
    assert_eq!(files.len(), 2);
}

#[test]
fn scan_excludes_hidden_directories() {
    let dir = tempfile::tempdir().unwrap();
    let hidden = dir.path().join(".hidden");
    fs::create_dir(&hidden).unwrap();
    fs::write(dir.path().join("visible.txt"), "v").unwrap();
    fs::write(hidden.join("secret.txt"), "s").unwrap();

    let crawler = Crawler::new(dir.path(), &[]);
    let files = crawler.scan().unwrap();
    assert_eq!(files.len(), 1);
    assert!(files[0].to_str().unwrap().contains("visible.txt"));
}
