use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct FileId(String);

impl FileId {
    pub fn new(id: String) -> Self {
        Self(id)
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct MimeType(String);

impl MimeType {
    pub fn new(mime: String) -> Self {
        Self(mime)
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }
}
