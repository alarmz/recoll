//! IndexReader 封裝行為測試

use rn_search::reader::RnReader;
use rn_search::schema::RnSchema;
use rn_search::tokenizer::register_tokenizers;
use rn_search::writer::RnWriter;

fn setup_with_docs(docs: &[(&str, &str, &str, &str)]) -> (tantivy::Index, RnSchema) {
    let schema = RnSchema::build();
    let index = tantivy::Index::create_in_ram(schema.schema.clone());
    register_tokenizers(&index);
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    for (doc_id, path, filename, content) in docs {
        writer
            .add_document(doc_id, path, filename, content, 100, 1700000000)
            .unwrap();
    }
    writer.commit().unwrap();
    (index, schema)
}

#[test]
fn search_returns_matching_results() {
    let (index, schema) = setup_with_docs(&[
        ("d1", "/a.txt", "a.txt", "rust programming language"),
        ("d2", "/b.txt", "b.txt", "python programming language"),
    ]);

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("rust", 10).unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].doc_id, "d1");
}

#[test]
fn search_returns_empty_for_no_match() {
    let (index, schema) = setup_with_docs(&[("d1", "/a.txt", "a.txt", "hello world")]);

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("nonexistent", 10).unwrap();
    assert!(results.is_empty());
}

#[test]
fn search_respects_limit() {
    let (index, schema) = setup_with_docs(&[
        ("d1", "/a.txt", "a.txt", "rust is great"),
        ("d2", "/b.txt", "b.txt", "rust is fast"),
        ("d3", "/c.txt", "c.txt", "rust is safe"),
    ]);

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("rust", 2).unwrap();
    assert_eq!(results.len(), 2);
}

#[test]
fn reload_sees_newly_committed_documents() {
    let schema = RnSchema::build();
    let index = tantivy::Index::create_in_ram(schema.schema.clone());
    register_tokenizers(&index);

    // 先建立 reader（此時沒有文件）
    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("hello", 10).unwrap();
    assert!(results.is_empty());

    // 寫入新文件並 commit
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    writer
        .add_document("d1", "/a.txt", "a.txt", "hello world", 100, 1700000000)
        .unwrap();
    writer.commit().unwrap();

    // reload 後應能搜尋到新文件
    reader.reload().unwrap();
    let results = reader.search("hello", 10).unwrap();
    assert_eq!(results.len(), 1);
}
