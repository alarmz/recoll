use anyhow::Result;
use tantivy::query::Query;
use tantivy::schema::Field;
use tantivy::Searcher;

/// 搜尋結果摘要生成器
pub struct SnippetBuilder {
    max_fragment_chars: usize,
    highlight_open: String,
    highlight_close: String,
}

impl SnippetBuilder {
    pub fn new(max_fragment_chars: usize) -> Self {
        Self {
            max_fragment_chars,
            highlight_open: "<em>".to_string(),
            highlight_close: "</em>".to_string(),
        }
    }

    pub fn with_highlights(mut self, open: &str, close: &str) -> Self {
        self.highlight_open = open.to_string();
        self.highlight_close = close.to_string();
        self
    }

    pub fn generate(
        &self,
        searcher: &Searcher,
        query: &dyn Query,
        doc_addr: tantivy::DocAddress,
        field: Field,
    ) -> Result<String> {
        let mut snippet_gen = tantivy::SnippetGenerator::create(searcher, query, field)?;
        snippet_gen.set_max_num_chars(self.max_fragment_chars);

        let doc = searcher.doc::<tantivy::TantivyDocument>(doc_addr)?;
        let snippet = snippet_gen.snippet_from_doc(&doc);

        let html = snippet.to_html();
        // Replace default <b> tags with custom highlight tags
        let mut result = html
            .replace("<b>", &self.highlight_open)
            .replace("</b>", &self.highlight_close);

        // Truncate to approximately max_fragment_chars
        if result.len() > self.max_fragment_chars {
            let truncate_at = result
                .char_indices()
                .take(self.max_fragment_chars)
                .last()
                .map(|(i, c)| i + c.len_utf8())
                .unwrap_or(0);
            result.truncate(truncate_at);
        }

        Ok(result)
    }
}
