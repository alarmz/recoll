use super::VecTokenStream;
use std::sync::Arc;
use tantivy::tokenizer::{Token, Tokenizer};

/// 基於 jieba-rs 的中文分詞器
#[derive(Clone)]
pub struct JiebaTokenizer {
    jieba: Arc<jieba_rs::Jieba>,
}

impl Default for JiebaTokenizer {
    fn default() -> Self {
        Self::new()
    }
}

impl JiebaTokenizer {
    pub fn new() -> Self {
        Self {
            jieba: Arc::new(jieba_rs::Jieba::new()),
        }
    }
}

impl Tokenizer for JiebaTokenizer {
    type TokenStream<'a> = VecTokenStream;

    fn token_stream<'a>(&'a mut self, text: &'a str) -> VecTokenStream {
        let words = self.jieba.cut(text, true);
        let mut tokens = Vec::new();
        let mut offset = 0;
        for word in words {
            if word.trim().is_empty() {
                offset += word.len();
                continue;
            }
            let start = text[offset..]
                .find(word)
                .map(|i| i + offset)
                .unwrap_or(offset);
            let end = start + word.len();
            tokens.push(Token {
                offset_from: start,
                offset_to: end,
                position: tokens.len(),
                text: word.to_string(),
                position_length: 1,
            });
            offset = end;
        }
        VecTokenStream::new(tokens)
    }
}
