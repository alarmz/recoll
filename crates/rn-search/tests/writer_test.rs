//! IndexWriter 封裝行為測試

use rn_search::reader::RnReader;
use rn_search::schema::RnSchema;
use rn_search::tokenizer::register_tokenizers;
use rn_search::writer::RnWriter;

fn setup() -> (tantivy::Index, RnSchema) {
    let schema = RnSchema::build();
    let index = tantivy::Index::create_in_ram(schema.schema.clone());
    register_tokenizers(&index);
    (index, schema)
}

#[test]
fn add_document_and_commit_makes_it_searchable() {
    let (index, schema) = setup();
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    writer
        .add_document(
            "doc-1",
            "/tmp/hello.txt",
            "hello.txt",
            "hello world",
            100,
            1700000000,
        )
        .unwrap();
    writer.commit().unwrap();

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("hello", 10).unwrap();
    assert_eq!(results.len(), 1);
    assert_eq!(results[0].doc_id, "doc-1");
}

#[test]
fn delete_by_doc_id_removes_document() {
    let (index, schema) = setup();
    let mut writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    writer
        .add_document(
            "doc-1",
            "/tmp/hello.txt",
            "hello.txt",
            "hello world",
            100,
            1700000000,
        )
        .unwrap();
    writer.commit().unwrap();

    // 刪除後重新 commit
    writer.delete_by_doc_id("doc-1").unwrap();
    writer.commit().unwrap();

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("hello", 10).unwrap();
    assert!(results.is_empty());
}

#[test]
fn uncommitted_documents_are_not_searchable() {
    let (index, schema) = setup();
    let writer = RnWriter::new(&index, &schema, 50_000_000).unwrap();
    writer
        .add_document(
            "doc-1",
            "/tmp/hello.txt",
            "hello.txt",
            "hello world",
            100,
            1700000000,
        )
        .unwrap();
    // 不 commit

    let reader = RnReader::new(&index, &schema).unwrap();
    let results = reader.search("hello", 10).unwrap();
    assert!(results.is_empty());
}
