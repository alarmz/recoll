//! MetaStore 行為測試

use std::path::Path;
use rn_meta::store::MetaStore;
use rn_meta::models::{MetadataRecord, FileMeta, IndexStats};
use rn_core::state::DocumentState;

fn open_temp_store() -> (MetaStore, tempfile::TempDir) {
    let dir = tempfile::tempdir().unwrap();
    let db_path = dir.path().join("test.db");
    let store = MetaStore::open(&db_path).unwrap();
    (store, dir)
}

fn sample_record(path: &str) -> MetadataRecord {
    MetadataRecord {
        file_id: uuid::Uuid::new_v4().to_string(),
        path: path.into(),
        filename: Path::new(path).file_name().unwrap().to_string_lossy().into(),
        extension: Path::new(path).extension().map(|e| e.to_string_lossy().into()),
        size: 1024,
        modified_at: 1700000000,
        mime_type: Some("text/plain".into()),
        index_state: "discovered".into(),
    }
}

// --- open ---

#[test]
fn open_creates_database_and_tables() {
    let (store, _dir) = open_temp_store();
    // 如果 open 成功且表已建立，upsert 不應 panic
    let rec = sample_record("/tmp/test.txt");
    store.upsert_metadata(&rec).unwrap();
}

// --- upsert_metadata ---

#[test]
fn upsert_inserts_new_record() {
    let (store, _dir) = open_temp_store();
    let rec = sample_record("/tmp/hello.txt");
    store.upsert_metadata(&rec).unwrap();

    let meta = store.get_file_meta("/tmp/hello.txt").unwrap();
    assert!(meta.is_some());
    let meta = meta.unwrap();
    assert_eq!(meta.filename, "hello.txt");
    assert_eq!(meta.size, 1024);
}

#[test]
fn upsert_updates_existing_record_on_conflict() {
    let (store, _dir) = open_temp_store();
    let mut rec = sample_record("/tmp/update.txt");
    store.upsert_metadata(&rec).unwrap();

    rec.size = 2048;
    rec.modified_at = 1700001000;
    store.upsert_metadata(&rec).unwrap();

    let meta = store.get_file_meta("/tmp/update.txt").unwrap().unwrap();
    assert_eq!(meta.size, 2048);
    assert_eq!(meta.modified_at, 1700001000);
}

// --- is_up_to_date ---

#[test]
fn is_up_to_date_returns_true_when_mtime_and_size_match() {
    let (store, _dir) = open_temp_store();
    let rec = sample_record("/tmp/current.txt");
    store.upsert_metadata(&rec).unwrap();

    assert!(store.is_up_to_date("/tmp/current.txt", 1700000000, 1024).unwrap());
}

#[test]
fn is_up_to_date_returns_false_when_mtime_differs() {
    let (store, _dir) = open_temp_store();
    let rec = sample_record("/tmp/old.txt");
    store.upsert_metadata(&rec).unwrap();

    assert!(!store.is_up_to_date("/tmp/old.txt", 1700099999, 1024).unwrap());
}

#[test]
fn is_up_to_date_returns_false_when_size_differs() {
    let (store, _dir) = open_temp_store();
    let rec = sample_record("/tmp/resized.txt");
    store.upsert_metadata(&rec).unwrap();

    assert!(!store.is_up_to_date("/tmp/resized.txt", 1700000000, 9999).unwrap());
}

#[test]
fn is_up_to_date_returns_false_for_unknown_path() {
    let (store, _dir) = open_temp_store();
    assert!(!store.is_up_to_date("/no/such/file.txt", 0, 0).unwrap());
}

// --- search_filename_prefix ---

#[test]
fn search_filename_prefix_finds_matching_files() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/docs/report_2024.pdf")).unwrap();
    store.upsert_metadata(&sample_record("/docs/report_2025.pdf")).unwrap();
    store.upsert_metadata(&sample_record("/docs/readme.md")).unwrap();

    let results = store.search_filename_prefix("report", 10).unwrap();
    assert_eq!(results.len(), 2);
}

#[test]
fn search_filename_prefix_respects_limit() {
    let (store, _dir) = open_temp_store();
    for i in 0..10 {
        store.upsert_metadata(&sample_record(&format!("/docs/file_{}.txt", i))).unwrap();
    }

    let results = store.search_filename_prefix("file_", 3).unwrap();
    assert_eq!(results.len(), 3);
}

#[test]
fn search_filename_prefix_returns_empty_for_no_match() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/docs/hello.txt")).unwrap();

    let results = store.search_filename_prefix("zzz", 10).unwrap();
    assert!(results.is_empty());
}

// --- set_state / get_doc_id ---

#[test]
fn set_state_updates_document_state() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/tmp/state.txt")).unwrap();

    store.set_state("/tmp/state.txt", &DocumentState::Indexed).unwrap();
    let meta = store.get_file_meta("/tmp/state.txt").unwrap().unwrap();
    assert_eq!(meta.index_state, "indexed");
}

#[test]
fn set_state_records_error_for_failed_state() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/tmp/fail.txt")).unwrap();

    let failed = DocumentState::Failed { error: "parse error".into(), retry_count: 2 };
    store.set_state("/tmp/fail.txt", &failed).unwrap();

    let meta = store.get_file_meta("/tmp/fail.txt").unwrap().unwrap();
    assert_eq!(meta.index_state, "failed");
}

#[test]
fn get_doc_id_returns_none_before_indexing() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/tmp/new.txt")).unwrap();

    assert!(store.get_doc_id("/tmp/new.txt").unwrap().is_none());
}

#[test]
fn get_doc_id_returns_value_after_set() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/tmp/indexed.txt")).unwrap();
    store.set_doc_id("/tmp/indexed.txt", "tantivy-doc-42").unwrap();

    let doc_id = store.get_doc_id("/tmp/indexed.txt").unwrap();
    assert_eq!(doc_id.as_deref(), Some("tantivy-doc-42"));
}

// --- delete_by_path ---

#[test]
fn delete_by_path_removes_record() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/tmp/gone.txt")).unwrap();

    store.delete_by_path("/tmp/gone.txt").unwrap();
    assert!(store.get_file_meta("/tmp/gone.txt").unwrap().is_none());
}

#[test]
fn delete_by_path_is_idempotent() {
    let (store, _dir) = open_temp_store();
    // 刪除不存在的路徑不應報錯
    store.delete_by_path("/no/such/file.txt").unwrap();
}

// --- find_stale ---

#[test]
fn find_stale_returns_stale_documents() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/project/a.rs")).unwrap();
    store.upsert_metadata(&sample_record("/project/b.rs")).unwrap();

    store.set_state("/project/a.rs", &DocumentState::Stale).unwrap();

    let stale = store.find_stale("/project").unwrap();
    assert_eq!(stale.len(), 1);
    assert_eq!(stale[0].path, "/project/a.rs");
}

// --- get_stats ---

#[test]
fn get_stats_counts_files_by_state() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/s/a.txt")).unwrap();
    store.upsert_metadata(&sample_record("/s/b.txt")).unwrap();
    store.upsert_metadata(&sample_record("/s/c.txt")).unwrap();

    store.set_state("/s/a.txt", &DocumentState::Indexed).unwrap();
    store.set_state("/s/b.txt", &DocumentState::Indexed).unwrap();

    let stats = store.get_stats().unwrap();
    assert_eq!(stats.total_files, 3);
    assert_eq!(stats.indexed_files, 2);
}

// --- get_failed_tasks ---

#[test]
fn get_failed_tasks_returns_failed_records() {
    let (store, _dir) = open_temp_store();
    store.upsert_metadata(&sample_record("/f/ok.txt")).unwrap();
    store.upsert_metadata(&sample_record("/f/bad.txt")).unwrap();

    store.set_state("/f/ok.txt", &DocumentState::Indexed).unwrap();
    let failed = DocumentState::Failed { error: "timeout".into(), retry_count: 1 };
    store.set_state("/f/bad.txt", &failed).unwrap();

    let failures = store.get_failed_tasks(10).unwrap();
    assert_eq!(failures.len(), 1);
    assert_eq!(failures[0].path, "/f/bad.txt");
}
