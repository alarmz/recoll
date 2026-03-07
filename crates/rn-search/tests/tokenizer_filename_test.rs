//! rn_filename tokenizer 行為測試 (檔名分詞)

mod common;

#[test]
fn filename_tokenizer_splits_on_hyphens() {
    let tokens = common::tokenize("rn_filename", "my-file-name");
    assert_eq!(tokens, vec!["my", "file", "name"]);
}

#[test]
fn filename_tokenizer_splits_on_underscores() {
    let tokens = common::tokenize("rn_filename", "my_file_name");
    assert_eq!(tokens, vec!["my", "file", "name"]);
}

#[test]
fn filename_tokenizer_splits_on_dots() {
    let tokens = common::tokenize("rn_filename", "report.2024.final");
    assert_eq!(tokens, vec!["report", "2024", "final"]);
}
