pub mod code;
pub mod filename;
pub mod jieba;

use tantivy::tokenizer::{
    LowerCaser, SimpleTokenizer, Stemmer, StopWordFilter, TextAnalyzer, Token, TokenStream,
};

/// 共用的 Vec-based TokenStream，供所有自訂 tokenizer 使用
pub struct VecTokenStream {
    tokens: Vec<Token>,
    index: i64,
}

impl VecTokenStream {
    pub fn new(tokens: Vec<Token>) -> Self {
        Self { tokens, index: -1 }
    }
}

impl TokenStream for VecTokenStream {
    fn advance(&mut self) -> bool {
        self.index += 1;
        (self.index as usize) < self.tokens.len()
    }

    fn token(&self) -> &Token {
        &self.tokens[self.index as usize]
    }

    fn token_mut(&mut self) -> &mut Token {
        &mut self.tokens[self.index as usize]
    }
}

const ENGLISH_STOP_WORDS: &[&str] = &[
    "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "if", "in", "into", "is", "it",
    "no", "not", "of", "on", "or", "such", "that", "the", "their", "then", "there", "these",
    "they", "this", "to", "was", "will", "with",
];

/// 向 Tantivy Index 註冊所有自訂 tokenizer
pub fn register_tokenizers(index: &tantivy::Index) {
    let manager = index.tokenizers();

    manager.register(
        "rn_default",
        TextAnalyzer::builder(SimpleTokenizer::default())
            .filter(LowerCaser)
            .filter(StopWordFilter::remove(
                ENGLISH_STOP_WORDS.iter().map(|s| s.to_string()),
            ))
            .filter(Stemmer::new(tantivy::tokenizer::Language::English))
            .build(),
    );

    manager.register(
        "rn_cjk",
        TextAnalyzer::builder(jieba::JiebaTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );

    manager.register(
        "rn_code",
        TextAnalyzer::builder(code::CodeTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );

    manager.register(
        "rn_filename",
        TextAnalyzer::builder(filename::FilenameTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );
}
