/// 寫入/更新用的中繼資料記錄
pub struct MetadataRecord {
    pub file_id: String,
    pub path: String,
    pub filename: String,
    pub extension: Option<String>,
    pub size: i64,
    pub modified_at: i64,
    pub mime_type: Option<String>,
    pub index_state: String,
}

/// 查詢回傳用的檔案中繼資料
pub struct FileMeta {
    pub file_id: String,
    pub path: String,
    pub filename: String,
    pub extension: Option<String>,
    pub size: i64,
    pub modified_at: i64,
    pub mime_type: Option<String>,
    pub index_state: String,
    pub doc_id: Option<String>,
    pub last_error: Option<String>,
    pub retry_count: i32,
}

impl FileMeta {
    pub fn from_row(row: &rusqlite::Row) -> rusqlite::Result<Self> {
        Ok(Self {
            file_id: row.get(0)?,
            path: row.get(1)?,
            filename: row.get(2)?,
            extension: row.get(3)?,
            size: row.get(4)?,
            modified_at: row.get(5)?,
            mime_type: row.get(6)?,
            index_state: row.get(7)?,
            doc_id: row.get(8)?,
            last_error: row.get(9)?,
            retry_count: row.get(10)?,
        })
    }
}

/// 索引統計
pub struct IndexStats {
    pub total_files: usize,
    pub indexed_files: usize,
    pub failed_files: usize,
    pub stale_files: usize,
}
