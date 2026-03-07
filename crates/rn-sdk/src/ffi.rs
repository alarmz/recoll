/// FFI 回傳結果
#[repr(C)]
#[derive(Debug, Clone)]
pub struct FfiResult {
    pub is_ok: bool,
    pub error_msg: String,
    pub data: String,
}

impl FfiResult {
    /// 成功結果
    pub fn ok(data: String) -> Self {
        Self {
            is_ok: true,
            error_msg: String::new(),
            data,
        }
    }

    /// 錯誤結果
    pub fn err(msg: String) -> Self {
        Self {
            is_ok: false,
            error_msg: msg,
            data: String::new(),
        }
    }
}

/// FFI 搜尋結果
#[repr(C)]
#[derive(Debug, Clone)]
pub struct FfiSearchResult {
    pub total: usize,
    pub hits_json: String,
}

impl FfiSearchResult {
    /// 從命中清單建立
    pub fn new(hits: Vec<String>) -> Self {
        let total = hits.len();
        let hits_json = serde_json::to_string(&hits).unwrap_or_default();
        Self { total, hits_json }
    }
}
