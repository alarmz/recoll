//! Endpoint 路由定義測試

use rn_sdk::endpoint::{all_endpoints, Method};

#[test]
fn endpoints_contain_required_routes() {
    let endpoints = all_endpoints();
    let paths: Vec<&str> = endpoints.iter().map(|e| e.path.as_str()).collect();
    assert!(paths.contains(&"/api/v1/search"));
    assert!(paths.contains(&"/api/v1/health"));
    assert!(paths.contains(&"/api/v1/stats"));
}

#[test]
fn search_endpoint_is_post() {
    let endpoints = all_endpoints();
    let search = endpoints
        .iter()
        .find(|e| e.path == "/api/v1/search")
        .unwrap();
    assert_eq!(search.method, Method::Post);
}

#[test]
fn health_endpoint_is_get() {
    let endpoints = all_endpoints();
    let health = endpoints
        .iter()
        .find(|e| e.path == "/api/v1/health")
        .unwrap();
    assert_eq!(health.method, Method::Get);
}
