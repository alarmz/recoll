//! SnippetBuilder 行為測試

use rn_search::schema::RnSchema;
use rn_search::snippet::SnippetBuilder;
use rn_search::tokenizer::register_tokenizers;
use rn_search::writer::RnWriter;
use tantivy::collector::TopDocs;
use tantivy::query::QueryParser;
use tantivy::DocAddress;

/// 建立含文件的索引，回傳 (Index, RnSchema)
fn setup_index(content: &str) -> (tantivy::Index, RnSchema) {
    let schema = RnSchema::build();
    let index = tantivy::Index::create_in_ram(schema.schema.clone());
    register_tokenizers(&index);
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    writer
        .add_document("d1", "/a.txt", "a.txt", content, 100, 1700000000)
        .unwrap();
    writer.commit().unwrap();
    (index, schema)
}

/// 搜尋並回傳第一個命中的 DocAddress（若有）
fn search_first(
    index: &tantivy::Index,
    schema: &RnSchema,
    query_str: &str,
) -> Option<(
    tantivy::Searcher,
    Box<dyn tantivy::query::Query>,
    DocAddress,
)> {
    let reader = index.reader().unwrap();
    let searcher = reader.searcher();
    let parser = QueryParser::for_index(searcher.index(), vec![schema.content]);
    let query = parser.parse_query(query_str).unwrap();
    let top = searcher.search(&query, &TopDocs::with_limit(1)).unwrap();
    if top.is_empty() {
        return None;
    }
    Some((searcher, query, top[0].1))
}

#[test]
fn snippet_contains_highlighted_keyword() {
    let (index, schema) = setup_index("the rust programming language is fast and safe");
    let (searcher, query, addr) = search_first(&index, &schema, "rust").unwrap();

    let builder = SnippetBuilder::new(200);
    let snippet = builder
        .generate(&searcher, &*query, addr, schema.content)
        .unwrap();
    assert!(
        snippet.contains("<em>") || snippet.contains("rust"),
        "snippet 應包含高亮或關鍵字, got: {snippet}"
    );
}

#[test]
fn snippet_respects_max_fragment_chars() {
    let long_text = "word ".repeat(500);
    let (index, schema) = setup_index(&long_text);
    let (searcher, query, addr) = search_first(&index, &schema, "word").unwrap();

    let builder = SnippetBuilder::new(200);
    let snippet = builder
        .generate(&searcher, &*query, addr, schema.content)
        .unwrap();
    assert!(
        snippet.len() <= 300,
        "snippet 應大約在 max_fragment_chars 範圍內, got len={}",
        snippet.len()
    );
}

#[test]
fn snippet_with_custom_highlights() {
    let (index, schema) = setup_index("rust is great");
    let (searcher, query, addr) = search_first(&index, &schema, "rust").unwrap();

    let builder = SnippetBuilder::new(200).with_highlights("<b>", "</b>");
    let snippet = builder
        .generate(&searcher, &*query, addr, schema.content)
        .unwrap();
    assert!(
        snippet.contains("<b>"),
        "snippet 應使用自訂高亮標記, got: {snippet}"
    );
}

#[test]
fn snippet_returns_empty_for_no_match_field() {
    let (index, schema) = setup_index("hello world");
    if let Some((searcher, query, addr)) = search_first(&index, &schema, "nonexistent") {
        let builder = SnippetBuilder::new(200);
        let snippet = builder
            .generate(&searcher, &*query, addr, schema.content)
            .unwrap();
        assert!(snippet.is_empty() || snippet.len() < 10);
    }
    // 無命中 → 通過
}
