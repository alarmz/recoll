use crate::schema::RnSchema;
use anyhow::Result;
use tantivy::collector::TopDocs;
use tantivy::query::QueryParser;
use tantivy::schema::Value;
use tantivy::{Index, IndexReader, ReloadPolicy};

/// 搜尋結果
pub struct SearchHit {
    pub doc_id: String,
    pub path: String,
    pub filename: String,
    pub score: f32,
}

/// Tantivy IndexReader 封裝
pub struct RnReader {
    reader: IndexReader,
    schema: RnSchema,
}

impl RnReader {
    pub fn new(index: &Index, schema: &RnSchema) -> Result<Self> {
        let reader = index
            .reader_builder()
            .reload_policy(ReloadPolicy::Manual)
            .try_into()?;
        Ok(Self {
            reader,
            schema: schema.clone(),
        })
    }

    pub fn search(&self, query_str: &str, limit: usize) -> Result<Vec<SearchHit>> {
        let searcher = self.reader.searcher();
        let query_parser = QueryParser::for_index(
            searcher.index(),
            vec![self.schema.content, self.schema.filename],
        );
        let query = query_parser.parse_query(query_str)?;
        let top_docs = searcher.search(&query, &TopDocs::with_limit(limit))?;

        let mut hits = Vec::new();
        for (score, doc_addr) in top_docs {
            let doc = searcher.doc::<tantivy::TantivyDocument>(doc_addr)?;
            let doc_id = doc
                .get_first(self.schema.doc_id)
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let path = doc
                .get_first(self.schema.path)
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            let filename = doc
                .get_first(self.schema.filename)
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();
            hits.push(SearchHit {
                doc_id,
                path,
                filename,
                score,
            });
        }
        Ok(hits)
    }

    pub fn reload(&self) -> Result<()> {
        self.reader.reload()?;
        Ok(())
    }
}
