use crate::schema::RnSchema;
use anyhow::Result;
use tantivy::query::{BooleanQuery, Query, RangeQuery, RegexQuery, TermQuery};
use tantivy::schema::IndexRecordOption;
use tantivy::{Index, Term};

/// 自訂查詢解析器，支援 ext:, path:, size:, modified: 等過濾語法
pub struct RnQueryParser {
    tantivy_parser: tantivy::query::QueryParser,
    schema: RnSchema,
}

impl RnQueryParser {
    pub fn new(index: &Index, schema: &RnSchema) -> Self {
        let tantivy_parser =
            tantivy::query::QueryParser::for_index(index, vec![schema.content, schema.filename]);
        Self {
            tantivy_parser,
            schema: schema.clone(),
        }
    }

    pub fn parse(&self, input: &str) -> Result<Box<dyn Query>> {
        let mut text_parts = Vec::new();
        let mut filters: Vec<Box<dyn Query>> = Vec::new();

        for token in tokenize_input(input) {
            if let Some(ext) = token.strip_prefix("ext:") {
                filters.push(Box::new(TermQuery::new(
                    Term::from_field_text(self.schema.extension, ext),
                    IndexRecordOption::Basic,
                )));
            } else if let Some(path_prefix) = token.strip_prefix("path:") {
                let escaped = escape_regex(path_prefix);
                filters.push(Box::new(
                    RegexQuery::from_pattern(&format!("{escaped}.*"), self.schema.path)
                        .unwrap_or_else(|_| {
                            RegexQuery::from_pattern(".*", self.schema.path).unwrap()
                        }),
                ));
            } else if let Some(size_expr) = token.strip_prefix("size:") {
                if let Some(filter) = parse_size_filter(size_expr) {
                    filters.push(filter);
                }
            } else if let Some(mod_expr) = token.strip_prefix("modified:") {
                if let Some(filter) = parse_modified_filter(mod_expr) {
                    filters.push(filter);
                }
            } else {
                text_parts.push(token);
            }
        }

        let text_query_str = text_parts.join(" ");
        let text_query = self.tantivy_parser.parse_query(&text_query_str)?;

        if filters.is_empty() {
            return Ok(text_query);
        }

        let mut clauses: Vec<(tantivy::query::Occur, Box<dyn Query>)> =
            vec![(tantivy::query::Occur::Must, text_query)];
        for f in filters {
            clauses.push((tantivy::query::Occur::Must, f));
        }

        Ok(Box::new(BooleanQuery::new(clauses)))
    }
}

/// Split input preserving quoted phrases and filter tokens
fn tokenize_input(input: &str) -> Vec<String> {
    let mut tokens = Vec::new();
    let mut chars = input.chars().peekable();
    let mut current = String::new();

    while let Some(&c) = chars.peek() {
        if c == '"' {
            if !current.is_empty() {
                tokens.push(current.clone());
                current.clear();
            }
            let mut phrase = String::new();
            phrase.push(chars.next().unwrap());
            while let Some(&nc) = chars.peek() {
                phrase.push(chars.next().unwrap());
                if nc == '"' {
                    break;
                }
            }
            tokens.push(phrase);
        } else if c.is_whitespace() {
            chars.next();
            if !current.is_empty() {
                tokens.push(current.clone());
                current.clear();
            }
        } else {
            current.push(chars.next().unwrap());
        }
    }
    if !current.is_empty() {
        tokens.push(current);
    }
    tokens
}

fn parse_size_filter(expr: &str) -> Option<Box<dyn Query>> {
    let val_str = expr.strip_prefix('>')?;
    let val = val_str.parse::<u64>().ok()?;
    Some(Box::new(RangeQuery::new_u64_bounds(
        "size_bytes".to_string(),
        std::ops::Bound::Excluded(val),
        std::ops::Bound::Unbounded,
    )))
}

fn parse_modified_filter(expr: &str) -> Option<Box<dyn Query>> {
    let date_str = expr.strip_prefix('>')?;
    let ts = parse_date(date_str)?;
    Some(Box::new(RangeQuery::new_i64_bounds(
        "modified_at".to_string(),
        std::ops::Bound::Included(ts),
        std::ops::Bound::Unbounded,
    )))
}

fn parse_date(date_str: &str) -> Option<i64> {
    let parts: Vec<&str> = date_str.split('-').collect();
    if parts.len() != 3 {
        return None;
    }
    let year: i32 = parts[0].parse().ok()?;
    let month: u32 = parts[1].parse().ok()?;
    let day: u32 = parts[2].parse().ok()?;
    Some(days_from_civil(year, month, day) * 86400)
}

fn escape_regex(s: &str) -> String {
    let mut out = String::with_capacity(s.len() * 2);
    for c in s.chars() {
        if "\\.*+?|[]{}()^$/".contains(c) {
            out.push('\\');
        }
        out.push(c);
    }
    out
}

/// Convert year/month/day to days since Unix epoch
fn days_from_civil(y: i32, m: u32, d: u32) -> i64 {
    let y = if m <= 2 { y - 1 } else { y } as i64;
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = (y - era * 400) as u64;
    let m = m as u64;
    let doy = if m > 2 {
        (153 * (m - 3) + 2) / 5 + d as u64 - 1
    } else {
        (153 * (m + 9) + 2) / 5 + d as u64 - 1
    };
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146097 + doe as i64 - 719468
}
