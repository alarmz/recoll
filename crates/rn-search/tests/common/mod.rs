use rn_search::schema::RnSchema;
use rn_search::tokenizer::register_tokenizers;
use tantivy::tokenizer::TextAnalyzer;

/// 使用指定的 tokenizer 對文字進行分詞，回傳 token 字串列表
pub fn tokenize(tokenizer_name: &str, text: &str) -> Vec<String> {
    let s = RnSchema::build();
    let index = tantivy::Index::create_in_ram(s.schema);
    register_tokenizers(&index);
    let mut tokenizer: TextAnalyzer = index.tokenizers().get(tokenizer_name).unwrap();
    let mut stream = tokenizer.token_stream(text);
    let mut tokens = vec![];
    while stream.advance() {
        tokens.push(stream.token().text.clone());
    }
    tokens
}
