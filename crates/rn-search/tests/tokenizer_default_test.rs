//! rn_default tokenizer 行為測試

mod common;

use rn_search::schema::RnSchema;
use rn_search::tokenizer::register_tokenizers;

#[test]
fn default_tokenizer_splits_english_words() {
    let tokens = common::tokenize("rn_default", "Hello World");
    assert!(tokens.contains(&"hello".to_string()));
    assert!(tokens.contains(&"world".to_string()));
}

#[test]
fn default_tokenizer_filters_stop_words() {
    let tokens = common::tokenize("rn_default", "the quick brown fox");
    assert!(!tokens.contains(&"the".to_string()));
    assert!(tokens.contains(&"quick".to_string()));
}

#[test]
fn default_tokenizer_applies_stemming() {
    let tokens = common::tokenize("rn_default", "running");
    assert!(tokens.contains(&"run".to_string()));
}

#[test]
fn default_tokenizer_is_registered_on_index() {
    let s = RnSchema::build();
    let index = tantivy::Index::create_in_ram(s.schema);
    register_tokenizers(&index);
    assert!(index.tokenizers().get("rn_default").is_some());
}
