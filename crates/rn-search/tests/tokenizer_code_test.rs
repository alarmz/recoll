//! rn_code tokenizer 行為測試 (程式碼分詞)

mod common;

#[test]
fn code_tokenizer_splits_camel_case() {
    let tokens = common::tokenize("rn_code", "getUserName");
    assert_eq!(tokens, vec!["get", "user", "name"]);
}

#[test]
fn code_tokenizer_splits_snake_case() {
    let tokens = common::tokenize("rn_code", "get_user_name");
    assert_eq!(tokens, vec!["get", "user", "name"]);
}

#[test]
fn code_tokenizer_splits_pascal_case() {
    let tokens = common::tokenize("rn_code", "HttpResponse");
    assert_eq!(tokens, vec!["http", "response"]);
}
