/// HTTP 方法
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Method {
    Get,
    Post,
}

/// API 端點定義
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Endpoint {
    pub path: String,
    pub method: Method,
    pub description: String,
}

/// 取得所有已定義的端點
pub fn all_endpoints() -> Vec<Endpoint> {
    vec![
        Endpoint {
            path: "/api/v1/search".to_string(),
            method: Method::Post,
            description: "Full-text search".to_string(),
        },
        Endpoint {
            path: "/api/v1/health".to_string(),
            method: Method::Get,
            description: "Health check".to_string(),
        },
        Endpoint {
            path: "/api/v1/stats".to_string(),
            method: Method::Get,
            description: "Index statistics".to_string(),
        },
        Endpoint {
            path: "/api/v1/index".to_string(),
            method: Method::Post,
            description: "Trigger indexing".to_string(),
        },
        Endpoint {
            path: "/api/v1/config".to_string(),
            method: Method::Get,
            description: "Current configuration".to_string(),
        },
    ]
}
