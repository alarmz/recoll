use super::VecTokenStream;
use tantivy::tokenizer::{Token, Tokenizer};

/// 檔名分詞器，以 - _ . 分割
#[derive(Clone, Default)]
pub struct FilenameTokenizer;

impl FilenameTokenizer {
    pub fn new() -> Self {
        Self
    }
}

impl Tokenizer for FilenameTokenizer {
    type TokenStream<'a> = VecTokenStream;

    fn token_stream<'a>(&'a mut self, text: &'a str) -> VecTokenStream {
        let mut tokens = Vec::new();
        for part in text.split(['-', '_', '.']) {
            if part.is_empty() {
                continue;
            }
            tokens.push(Token {
                offset_from: 0,
                offset_to: part.len(),
                position: tokens.len(),
                text: part.to_string(),
                position_length: 1,
            });
        }
        VecTokenStream::new(tokens)
    }
}
