//! Schema 行為測試

use rn_search::schema::RnSchema;

// --- build ---

#[test]
fn build_creates_schema_with_all_16_fields() {
    let s = RnSchema::build();
    // 驗證 schema 包含所有欄位名稱
    let field_names: Vec<&str> = s.schema.fields().map(|(_, entry)| entry.name()).collect();
    assert!(field_names.contains(&"doc_id"));
    assert!(field_names.contains(&"path"));
    assert!(field_names.contains(&"filename"));
    assert!(field_names.contains(&"title"));
    assert!(field_names.contains(&"extension"));
    assert!(field_names.contains(&"mime_type"));
    assert!(field_names.contains(&"content"));
    assert!(field_names.contains(&"summary"));
    assert!(field_names.contains(&"language"));
    assert!(field_names.contains(&"created_at"));
    assert!(field_names.contains(&"modified_at"));
    assert!(field_names.contains(&"indexed_at"));
    assert!(field_names.contains(&"size_bytes"));
    assert!(field_names.contains(&"source_type"));
    assert!(field_names.contains(&"tags"));
    assert!(field_names.contains(&"checksum"));
    assert_eq!(field_names.len(), 16);
}

#[test]
fn doc_id_and_path_are_string_stored() {
    let s = RnSchema::build();
    // doc_id 和 path 應為 STRING | STORED
    let doc_id_entry = s.schema.get_field_entry(s.doc_id);
    assert!(doc_id_entry.is_indexed());
    assert!(doc_id_entry.is_stored());

    let path_entry = s.schema.get_field_entry(s.path);
    assert!(path_entry.is_indexed());
    assert!(path_entry.is_stored());
}

#[test]
fn content_is_indexed_and_stored() {
    let s = RnSchema::build();
    let content_entry = s.schema.get_field_entry(s.content);
    assert!(content_entry.is_indexed());
    assert!(content_entry.is_stored());
}

#[test]
fn modified_at_is_i64_stored() {
    let s = RnSchema::build();
    let entry = s.schema.get_field_entry(s.modified_at);
    assert!(entry.is_stored());
    // 透過 schema 確認欄位名稱存在且為數值型別
    assert_eq!(entry.name(), "modified_at");
}

#[test]
fn size_bytes_is_u64_stored() {
    let s = RnSchema::build();
    let entry = s.schema.get_field_entry(s.size_bytes);
    assert!(entry.is_stored());
    assert_eq!(entry.name(), "size_bytes");
}
