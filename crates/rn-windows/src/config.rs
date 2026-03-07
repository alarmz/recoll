/// Windows Service 設定
#[derive(Debug, Clone)]
pub struct ServiceConfig {
    pub name: String,
    pub display_name: String,
    pub description: String,
    pub binary_name: String,
}

impl Default for ServiceConfig {
    fn default() -> Self {
        Self {
            name: "RecollNext".to_string(),
            display_name: "Recoll Next Indexing Service".to_string(),
            description: "Recoll Next desktop full-text search indexing service".to_string(),
            binary_name: "rn-cli.exe".to_string(),
        }
    }
}
