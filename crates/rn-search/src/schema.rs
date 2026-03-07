use tantivy::schema::*;

/// Tantivy schema 定義，包含所有搜尋欄位
#[derive(Clone)]
pub struct RnSchema {
    pub schema: Schema,
    pub doc_id: Field,
    pub path: Field,
    pub filename: Field,
    pub title: Field,
    pub extension: Field,
    pub mime_type: Field,
    pub content: Field,
    pub summary: Field,
    pub language: Field,
    pub created_at: Field,
    pub modified_at: Field,
    pub indexed_at: Field,
    pub size_bytes: Field,
    pub source_type: Field,
    pub tags: Field,
    pub checksum: Field,
}

impl RnSchema {
    pub fn build() -> Self {
        let mut builder = Schema::builder();

        let text_stored = TextOptions::default()
            .set_indexing_options(
                TextFieldIndexing::default()
                    .set_tokenizer("rn_default")
                    .set_index_option(IndexRecordOption::WithFreqsAndPositions),
            )
            .set_stored();

        let text_not_stored = TextOptions::default().set_indexing_options(
            TextFieldIndexing::default()
                .set_tokenizer("rn_default")
                .set_index_option(IndexRecordOption::WithFreqsAndPositions),
        );

        let i64_fast = NumericOptions::default().set_fast().set_stored();
        let u64_fast = NumericOptions::default().set_fast().set_stored();

        let doc_id = builder.add_text_field("doc_id", STRING | STORED);
        let path = builder.add_text_field("path", STRING | STORED);
        let filename = builder.add_text_field("filename", text_stored.clone());
        let title = builder.add_text_field("title", text_stored.clone());
        let extension = builder.add_text_field("extension", STRING | STORED | FAST);
        let mime_type = builder.add_text_field("mime_type", STRING | STORED);
        let content = builder.add_text_field("content", text_not_stored);
        let summary = builder.add_text_field("summary", STORED);
        let language = builder.add_text_field("language", STRING | STORED);
        let created_at = builder.add_i64_field("created_at", i64_fast.clone());
        let modified_at = builder.add_i64_field("modified_at", i64_fast.clone());
        let indexed_at = builder.add_i64_field("indexed_at", i64_fast);
        let size_bytes = builder.add_u64_field("size_bytes", u64_fast);
        let source_type = builder.add_text_field("source_type", STRING | STORED);
        let tags = builder.add_text_field("tags", text_stored);
        let checksum = builder.add_text_field("checksum", STRING | STORED);

        Self {
            schema: builder.build(),
            doc_id,
            path,
            filename,
            title,
            extension,
            mime_type,
            content,
            summary,
            language,
            created_at,
            modified_at,
            indexed_at,
            size_bytes,
            source_type,
            tags,
            checksum,
        }
    }
}
