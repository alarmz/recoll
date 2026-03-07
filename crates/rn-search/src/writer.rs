use crate::schema::RnSchema;
use anyhow::Result;
use tantivy::{doc, Index, IndexWriter, Term};

/// Tantivy IndexWriter 封裝
pub struct RnWriter {
    writer: IndexWriter,
    schema: RnSchema,
}

impl RnWriter {
    pub fn new(index: &Index, schema: &RnSchema, heap_size: usize) -> Result<Self> {
        let writer = index.writer(heap_size)?;
        Ok(Self {
            writer,
            schema: schema.clone(),
        })
    }

    pub fn add_document(
        &self,
        doc_id: &str,
        path: &str,
        filename: &str,
        content: &str,
        size_bytes: u64,
        modified_at: i64,
    ) -> Result<()> {
        self.add_document_full(doc_id, path, filename, "", content, size_bytes, modified_at)
    }

    #[allow(clippy::too_many_arguments)]
    pub fn add_document_full(
        &self,
        doc_id: &str,
        path: &str,
        filename: &str,
        extension: &str,
        content: &str,
        size_bytes: u64,
        modified_at: i64,
    ) -> Result<()> {
        let s = &self.schema;
        self.writer.add_document(doc!(
            s.doc_id => doc_id,
            s.path => path,
            s.filename => filename,
            s.extension => extension,
            s.content => content,
            s.size_bytes => size_bytes,
            s.modified_at => modified_at,
        ))?;
        Ok(())
    }

    pub fn delete_by_doc_id(&self, doc_id: &str) -> Result<()> {
        self.writer
            .delete_term(Term::from_field_text(self.schema.doc_id, doc_id));
        Ok(())
    }

    pub fn commit(&mut self) -> Result<()> {
        self.writer.commit()?;
        Ok(())
    }
}
