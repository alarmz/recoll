//! SearchRequest 驗證測試

use rn_sdk::request::SearchRequest;

#[test]
fn valid_request_passes_validation() {
    let req = SearchRequest {
        query: "hello".to_string(),
        limit: 20,
        offset: 0,
    };
    assert!(req.validate().is_ok());
}

#[test]
fn empty_query_fails_validation() {
    let req = SearchRequest {
        query: "".to_string(),
        limit: 20,
        offset: 0,
    };
    assert!(req.validate().is_err());
}

#[test]
fn zero_limit_fails_validation() {
    let req = SearchRequest {
        query: "hello".to_string(),
        limit: 0,
        offset: 0,
    };
    assert!(req.validate().is_err());
}
