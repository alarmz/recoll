use super::VecTokenStream;
use tantivy::tokenizer::{Token, Tokenizer};

/// 程式碼分詞器，支援 camelCase、snake_case、PascalCase 分割
#[derive(Clone, Default)]
pub struct CodeTokenizer;

impl CodeTokenizer {
    pub fn new() -> Self {
        Self
    }
}

impl Tokenizer for CodeTokenizer {
    type TokenStream<'a> = VecTokenStream;

    fn token_stream<'a>(&'a mut self, text: &'a str) -> VecTokenStream {
        let mut tokens = Vec::new();
        for part in text.split('_') {
            if part.is_empty() {
                continue;
            }
            for word in split_camel_case(part) {
                if word.is_empty() {
                    continue;
                }
                tokens.push(Token {
                    offset_from: 0,
                    offset_to: word.len(),
                    position: tokens.len(),
                    text: word,
                    position_length: 1,
                });
            }
        }
        VecTokenStream::new(tokens)
    }
}

fn split_camel_case(s: &str) -> Vec<String> {
    let mut words = Vec::new();
    let mut current = String::new();
    for c in s.chars() {
        if c.is_uppercase() && !current.is_empty() {
            words.push(current.clone());
            current.clear();
        }
        current.push(c);
    }
    if !current.is_empty() {
        words.push(current);
    }
    words
}
