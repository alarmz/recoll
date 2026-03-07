//! QueryParser 行為測試

use rn_search::query::RnQueryParser;
use rn_search::schema::RnSchema;
use rn_search::tokenizer::register_tokenizers;
use rn_search::writer::RnWriter;

/// 建立含文件的測試索引，回傳 (Index, RnSchema)
fn setup_index(docs: &[(&str, &str, &str, &str, &str, u64, i64)]) -> (tantivy::Index, RnSchema) {
    let schema = RnSchema::build();
    let index = tantivy::Index::create_in_ram(schema.schema.clone());
    register_tokenizers(&index);
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    for &(doc_id, path, filename, ext, content, size, modified) in docs {
        writer
            .add_document_full(doc_id, path, filename, ext, content, size, modified)
            .unwrap();
    }
    writer.commit().unwrap();
    (index, schema)
}

/// 用 RnQueryParser 解析查詢，搜尋並回傳命中的 doc_id 列表
fn search_with_query(index: &tantivy::Index, schema: &RnSchema, query_str: &str) -> Vec<String> {
    let parser = RnQueryParser::new(index, schema);
    let query = parser.parse(query_str).unwrap();
    let reader = index
        .reader_builder()
        .reload_policy(tantivy::ReloadPolicy::Manual)
        .try_into()
        .unwrap();
    let searcher = reader.searcher();
    let top_docs = searcher
        .search(&query, &tantivy::collector::TopDocs::with_limit(100))
        .unwrap();
    top_docs
        .into_iter()
        .map(|(_, addr)| {
            let doc = searcher.doc::<tantivy::TantivyDocument>(addr).unwrap();
            use tantivy::schema::Value;
            doc.get_first(schema.doc_id)
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string()
        })
        .collect()
}

// --- 關鍵字查詢 ---

#[test]
fn keyword_query_matches_content() {
    let (index, schema) = setup_index(&[
        (
            "d1",
            "/a.rs",
            "main.rs",
            "rs",
            "rust programming",
            100,
            1700000000,
        ),
        (
            "d2",
            "/b.py",
            "app.py",
            "py",
            "python scripting",
            200,
            1700000000,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "rust");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}

// --- 精確詞組 ---

#[test]
fn phrase_query_matches_exact_phrase() {
    let (index, schema) = setup_index(&[
        (
            "d1",
            "/a.txt",
            "a.txt",
            "txt",
            "hello world greeting",
            100,
            1700000000,
        ),
        (
            "d2",
            "/b.txt",
            "b.txt",
            "txt",
            "world hello reversed",
            100,
            1700000000,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "\"hello world\"");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}

// --- ext: 過濾 ---

#[test]
fn ext_filter_restricts_by_extension() {
    let (index, schema) = setup_index(&[
        ("d1", "/a.rs", "main.rs", "rs", "code here", 100, 1700000000),
        ("d2", "/b.py", "app.py", "py", "code here", 100, 1700000000),
    ]);
    let hits = search_with_query(&index, &schema, "code ext:rs");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}

// --- path: 過濾 ---

#[test]
fn path_filter_restricts_by_path_prefix() {
    let (index, schema) = setup_index(&[
        (
            "d1",
            "/project/src/main.rs",
            "main.rs",
            "rs",
            "code",
            100,
            1700000000,
        ),
        (
            "d2",
            "/docs/readme.md",
            "readme.md",
            "md",
            "code",
            100,
            1700000000,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "code path:/project");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}

// --- size: 過濾 ---

#[test]
fn size_filter_greater_than() {
    let (index, schema) = setup_index(&[
        ("d1", "/a.txt", "a.txt", "txt", "data", 500, 1700000000),
        ("d2", "/b.txt", "b.txt", "txt", "data", 2000, 1700000000),
    ]);
    let hits = search_with_query(&index, &schema, "data size:>1024");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d2".to_string()));
}

// --- modified: 過濾 ---

#[test]
fn modified_filter_after_date() {
    // 2024-01-01 00:00:00 UTC = 1704067200
    // 2023-06-01 = 1685577600
    let (index, schema) = setup_index(&[
        (
            "d1", "/old.txt", "old.txt", "txt", "content", 100, 1685577600,
        ),
        (
            "d2", "/new.txt", "new.txt", "txt", "content", 100, 1704067200,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "content modified:>2024-01-01");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d2".to_string()));
}

// --- AND 布林 ---

#[test]
fn boolean_and_requires_both_terms() {
    let (index, schema) = setup_index(&[
        ("d1", "/a.txt", "a.txt", "txt", "rust fast", 100, 1700000000),
        ("d2", "/b.txt", "b.txt", "txt", "rust safe", 100, 1700000000),
        (
            "d3",
            "/c.txt",
            "c.txt",
            "txt",
            "python fast",
            100,
            1700000000,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "rust AND fast");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}

// --- 混合查詢 ---

#[test]
fn mixed_keyword_and_ext_filter() {
    let (index, schema) = setup_index(&[
        (
            "d1",
            "/a.rs",
            "main.rs",
            "rs",
            "hello world",
            100,
            1700000000,
        ),
        (
            "d2",
            "/b.py",
            "app.py",
            "py",
            "hello world",
            100,
            1700000000,
        ),
        (
            "d3",
            "/c.rs",
            "lib.rs",
            "rs",
            "goodbye world",
            100,
            1700000000,
        ),
    ]);
    let hits = search_with_query(&index, &schema, "hello ext:rs");
    assert_eq!(hits.len(), 1);
    assert!(hits.contains(&"d1".to_string()));
}
