use std::path::Path;
use std::sync::{Arc, Mutex};
use anyhow::Result;
use rusqlite::{params, Connection};
use rn_core::state::DocumentState;
use crate::models::{MetadataRecord, FileMeta, IndexStats};

const SELECT_FILE_META: &str =
    "SELECT file_id, path, filename, extension, size, modified_at, mime_type, index_state, doc_id, last_error, retry_count FROM files";

pub struct MetaStore {
    conn: Arc<Mutex<Connection>>,
}

impl MetaStore {
    pub fn open(db_path: &Path) -> Result<Self> {
        let conn = Connection::open(db_path)?;
        conn.execute_batch(
            "PRAGMA journal_mode = WAL;
             PRAGMA synchronous = NORMAL;
             PRAGMA foreign_keys = ON;

             CREATE TABLE IF NOT EXISTS files (
                 file_id       TEXT PRIMARY KEY,
                 path          TEXT NOT NULL UNIQUE,
                 filename      TEXT NOT NULL,
                 extension     TEXT,
                 size          INTEGER NOT NULL,
                 modified_at   INTEGER NOT NULL,
                 mime_type     TEXT,
                 index_state   TEXT NOT NULL DEFAULT 'discovered',
                 doc_id        TEXT,
                 retry_count   INTEGER NOT NULL DEFAULT 0,
                 last_error    TEXT,
                 indexed_at    INTEGER,
                 updated_at    INTEGER NOT NULL DEFAULT (unixepoch())
             );

             CREATE INDEX IF NOT EXISTS idx_files_path      ON files(path);
             CREATE INDEX IF NOT EXISTS idx_files_filename   ON files(filename);
             CREATE INDEX IF NOT EXISTS idx_files_state      ON files(index_state);",
        )?;
        Ok(Self { conn: Arc::new(Mutex::new(conn)) })
    }

    pub fn upsert_metadata(&self, rec: &MetadataRecord) -> Result<()> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            "INSERT INTO files (file_id, path, filename, extension, size, modified_at, mime_type, index_state)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)
             ON CONFLICT(path) DO UPDATE SET
                 size = excluded.size,
                 modified_at = excluded.modified_at,
                 mime_type = excluded.mime_type,
                 updated_at = unixepoch()",
            params![rec.file_id, rec.path, rec.filename, rec.extension, rec.size, rec.modified_at, rec.mime_type, rec.index_state],
        )?;
        Ok(())
    }

    pub fn get_file_meta(&self, path: &str) -> Result<Option<FileMeta>> {
        let conn = self.conn.lock().unwrap();
        let sql = format!("{SELECT_FILE_META} WHERE path = ?1");
        let mut stmt = conn.prepare(&sql)?;
        let mut rows = stmt.query_map(params![path], FileMeta::from_row)?;
        match rows.next() {
            Some(row) => Ok(Some(row?)),
            None => Ok(None),
        }
    }

    pub fn is_up_to_date(&self, path: &str, mtime: i64, size: i64) -> Result<bool> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare(
            "SELECT 1 FROM files WHERE path = ?1 AND modified_at = ?2 AND size = ?3"
        )?;
        Ok(stmt.exists(params![path, mtime, size])?)
    }

    pub fn search_filename_prefix(&self, prefix: &str, limit: usize) -> Result<Vec<FileMeta>> {
        let conn = self.conn.lock().unwrap();
        let pattern = format!("{prefix}%");
        let sql = format!("{SELECT_FILE_META} WHERE filename LIKE ?1 LIMIT ?2");
        let mut stmt = conn.prepare(&sql)?;
        let rows = stmt.query_map(params![pattern, limit as i64], FileMeta::from_row)?;
        rows.collect::<std::result::Result<Vec<_>, _>>().map_err(Into::into)
    }

    pub fn set_state(&self, path: &str, state: &DocumentState) -> Result<()> {
        let conn = self.conn.lock().unwrap();
        let (error, retry) = match state {
            DocumentState::Failed { error, retry_count } => (Some(error.as_str()), *retry_count as i32),
            _ => (None, 0),
        };
        conn.execute(
            "UPDATE files SET index_state = ?1, last_error = ?2, retry_count = ?3, updated_at = unixepoch() WHERE path = ?4",
            params![state.as_str(), error, retry, path],
        )?;
        Ok(())
    }

    pub fn get_doc_id(&self, path: &str) -> Result<Option<String>> {
        let conn = self.conn.lock().unwrap();
        let mut stmt = conn.prepare("SELECT doc_id FROM files WHERE path = ?1")?;
        let mut rows = stmt.query_map(params![path], |row| row.get::<_, Option<String>>(0))?;
        match rows.next() {
            Some(row) => Ok(row?),
            None => Ok(None),
        }
    }

    pub fn set_doc_id(&self, path: &str, doc_id: &str) -> Result<()> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            "UPDATE files SET doc_id = ?1, updated_at = unixepoch() WHERE path = ?2",
            params![doc_id, path],
        )?;
        Ok(())
    }

    pub fn delete_by_path(&self, path: &str) -> Result<()> {
        let conn = self.conn.lock().unwrap();
        conn.execute("DELETE FROM files WHERE path = ?1", params![path])?;
        Ok(())
    }

    pub fn find_stale(&self, root: &str) -> Result<Vec<FileMeta>> {
        let conn = self.conn.lock().unwrap();
        let pattern = format!("{root}%");
        let sql = format!("{SELECT_FILE_META} WHERE index_state = 'stale' AND path LIKE ?1");
        let mut stmt = conn.prepare(&sql)?;
        let rows = stmt.query_map(params![pattern], FileMeta::from_row)?;
        rows.collect::<std::result::Result<Vec<_>, _>>().map_err(Into::into)
    }

    pub fn get_stats(&self) -> Result<IndexStats> {
        let conn = self.conn.lock().unwrap();
        let total: usize = conn.query_row("SELECT COUNT(*) FROM files", [], |r| r.get(0))?;
        let indexed: usize = conn.query_row("SELECT COUNT(*) FROM files WHERE index_state = 'indexed'", [], |r| r.get(0))?;
        let failed: usize = conn.query_row("SELECT COUNT(*) FROM files WHERE index_state = 'failed'", [], |r| r.get(0))?;
        let stale: usize = conn.query_row("SELECT COUNT(*) FROM files WHERE index_state = 'stale'", [], |r| r.get(0))?;
        Ok(IndexStats { total_files: total, indexed_files: indexed, failed_files: failed, stale_files: stale })
    }

    pub fn get_failed_tasks(&self, limit: usize) -> Result<Vec<FileMeta>> {
        let conn = self.conn.lock().unwrap();
        let sql = format!("{SELECT_FILE_META} WHERE index_state = 'failed' LIMIT ?1");
        let mut stmt = conn.prepare(&sql)?;
        let rows = stmt.query_map(params![limit as i64], FileMeta::from_row)?;
        rows.collect::<std::result::Result<Vec<_>, _>>().map_err(Into::into)
    }
}
