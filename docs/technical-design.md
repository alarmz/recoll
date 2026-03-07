# Recoll Next — Technical Design Document

> **Version**: v0.2-merged
> **Status**: Draft — 供工程團隊審核
> **Branch**: `recoll-next`
> **Core Language**: Rust edition 2021
> **Target Platform**: Windows 10/11/Server 2019+ x64（Linux 為次要支援）
> **Last Updated**: 2026-03-07

---

## 目錄

1. [概述與目標](#1-概述與目標)
2. [技術選型與依賴](#2-技術選型與依賴)
3. [Workspace 結構與模組邊界](#3-workspace-結構與模組邊界)
4. [核心資料模型 (rn-core)](#4-核心資料模型-rn-core)
5. [Indexer Pipeline (rn-indexer)](#5-indexer-pipeline-rn-indexer)
6. [Tantivy Schema + 查詢設計 (rn-search)](#6-tantivy-schema--查詢設計-rn-search)
7. [Metadata Index (rn-meta)](#7-metadata-index-rn-meta)
8. [文件內容抽取器 (rn-extractors)](#8-文件內容抽取器-rn-extractors)
9. [檔案監控 (rn-windows / notify)](#9-檔案監控-rn-windows--notify)
10. [查詢與排序](#10-查詢與排序)
11. [GPU 加速架構 (rn-gpu)](#11-gpu-加速架構-rn-gpu)
12. [Windows 平台整合 (rn-windows)](#12-windows-平台整合-rn-windows)
13. [GUI 設計 (rn-gui)](#13-gui-設計-rn-gui)
14. [CLI 設計 (rn-cli)](#14-cli-設計-rn-cli)
15. [SDK / Public API (rn-sdk)](#15-sdk--public-api-rn-sdk)
16. [設定系統](#16-設定系統)
17. [日誌與監控](#17-日誌與監控)
18. [錯誤處理與恢復](#18-錯誤處理與恢復)
19. [安全性](#19-安全性)
20. [測試策略](#20-測試策略)
21. [CI/CD Pipeline](#21-cicd-pipeline)
22. [效能目標與基準測試](#22-效能目標與基準測試)
23. [Phase 0 PoC 工作清單](#23-phase-0-poc-工作清單)
24. [里程碑與交付物](#24-里程碑與交付物)
25. [風險與緩解](#25-風險與緩解)
26. [附錄：HTTP API 參考](#26-附錄http-api-參考)

---

## 1. 概述與目標

### 1.1 系統定位

Recoll Next 是一個以 Rust + Tantivy 打造的新一代桌面全文搜尋系統，目標是成為 Windows-first 的高效能本機搜尋引擎。

### 1.2 核心架構決策

| 決策項目 | 選擇 | 理由 |
|---------|------|------|
| 核心語言 | Rust (edition 2021) | 記憶體安全、高效能並行、跨平台原生建置 |
| 搜尋引擎 | Tantivy 0.22 | Rust 原生、BM25、可控 schema、嵌入式部署 |
| Metadata 儲存 | SQLite (rusqlite, bundled) | 成熟穩定、零配置、WAL 模式高併發讀取 |
| GUI 框架 | Tauri 2.x | Web 技術 UI、Rust 後端、輕量 WebView |
| GPU 框架 | CUDA/ROCm backend trait | 可插拔、feature-gated、CPU fallback |
| 非同步執行 | tokio (full) | Rust 標準非同步 runtime |
| CPU-bound 並行 | rayon | Work-stealing thread pool |
| Worker 通訊 | crossbeam-channel | 無鎖高效能 channel |

### 1.3 不做的事（明確排除）

- 不重用現有 Recoll C++ 程式碼
- 不支援 Linux/macOS 作為第一期目標（但架構不阻擋）
- 不自研 OCR 引擎
- 不建置分散式叢集
- 不實作雲端同步

### 1.4 最低系統需求

| 項目 | 需求 |
|------|------|
| OS | Windows 10 x64 1903+ |
| RAM | 4 GB (建議 8 GB+) |
| Disk | 500 MB 安裝 + 索引空間（約原始資料 10-30%） |
| GPU | 選配，支援 CUDA / ROCm / Vulkan |
| Rust toolchain | 1.78+ (MSVC target) |

---

## 2. 技術選型與依賴

### 2.1 主要 Crate 版本鎖定

| Crate | 版本 | 用途 |
|-------|------|------|
| `tantivy` | 0.22 | 全文搜尋核心 |
| `tokio` | 1.x (full features) | 非同步執行時 |
| `rusqlite` | 0.31 + bundled | Metadata SQLite |
| `serde` / `serde_json` | 1.x | 序列化 |
| `toml` | 0.8 | 設定檔解析 |
| `tracing` / `tracing-subscriber` | 0.1.x / 0.3.x | 結構化日誌 |
| `tracing-appender` | 0.2 | 日誌檔案輪替 |
| `anyhow` | 1.x | 應用層錯誤 |
| `thiserror` | 1.x | 函式庫層錯誤 |
| `uuid` | 1.x (v4, serde) | Task ID |
| `crossbeam-channel` | 0.5 | Worker 溝通 |
| `rayon` | 1.x | CPU-bound 並行 |
| `notify` | 6.x | 跨平台 FSWatcher |
| `walkdir` | 2.x | 目錄遞迴走訪 |
| `windows-service` | 0.7 | Windows SCM |
| `tauri` | 2.x | GUI |
| `clap` | 4.x (derive) | CLI |
| `jieba-rs` | 0.7 | CJK 分詞 |
| `pdf-extract` | 0.7 | PDF text layer |
| `docx-rs` | 0.4 | DOCX 抽取 |
| `calamine` | 0.24 | Excel 讀取 |
| `quick-xml` | 0.31 | XML 解析 |
| `pulldown-cmark` | 0.10 | Markdown 解析 |
| `mime_guess` | 2.x | MIME 偵測 |
| `unicode-normalization` | 0.1 | Unicode NFC |
| `unicode-segmentation` | 1.x | Unicode 詞邊界 |
| `xxhash-rust` | 0.8 (xxh3) | 快速雜湊 |
| `glob` | 0.3 | 排除規則匹配 |
| `axum` | 0.7 | HTTP API (rn-sdk) |
| `criterion` | 0.5 | Benchmark |
| `tempfile` | 3.x | 測試用臨時檔案 |

### 2.2 外部工具依賴（可選）

| 工具 | 用途 | 必要性 |
|------|------|--------|
| Tesseract | OCR fallback | 選配 |
| LibreOffice | 複雜 Office 格式轉換 fallback | 選配 |
| poppler/pdftotext | PDF fallback extractor | 選配 |

---

## 3. Workspace 結構與模組邊界

### 3.1 Cargo Workspace 佈局

```
recoll-next/
├── Cargo.toml                    # [workspace] members = [...]
├── Cargo.lock
├── .github/
│   └── workflows/
│       ├── ci.yml                # PR/push: fmt, clippy, test
│       └── release.yml           # tag: build + installer + publish
├── crates/
│   ├── rn-core/                  # domain models, errors, config, events (lib)
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── config.rs         # 設定結構與載入
│   │       ├── error.rs          # 統一錯誤型別 (thiserror)
│   │       ├── event.rs          # 內部事件模型
│   │       ├── task.rs           # IndexTask, TaskPriority, OperationType
│   │       ├── state.rs          # DocumentState 狀態機
│   │       ├── extract.rs        # ExtractResult, Language, ExtractionMethod
│   │       ├── search.rs         # SearchResult, MatchReason, SourceType
│   │       └── types.rs          # FileId, MimeType 等共用型別
│   │
│   ├── rn-meta/                  # Metadata 索引 (SQLite)
│   │   ├── Cargo.toml
│   │   ├── migrations/
│   │   │   └── 001_init.sql
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── store.rs          # MetaStore 介面
│   │       └── models.rs         # MetadataRecord, FileMeta
│   │
│   ├── rn-extractors/            # 文件內容抽取
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs            # Extractor trait
│   │       ├── registry.rs       # ExtractorRegistry
│   │       ├── plain_text.rs
│   │       ├── pdf.rs
│   │       ├── docx.rs
│   │       ├── xlsx.rs
│   │       ├── pptx.rs
│   │       ├── html.rs
│   │       ├── markdown.rs
│   │       ├── source_code.rs
│   │       ├── email.rs
│   │       ├── csv.rs
│   │       └── fallback.rs       # 最後防線：嘗試 UTF-8 讀取
│   │
│   ├── rn-search/                # Tantivy 搜尋引擎
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── schema.rs         # RnSchema 定義
│   │       ├── writer.rs         # IndexWriter 封裝
│   │       ├── reader.rs         # IndexReader 與查詢
│   │       ├── query.rs          # QueryParser
│   │       ├── ranking.rs        # 混合排序
│   │       ├── snippet.rs        # 搜尋結果摘要
│   │       └── tokenizer/
│   │           ├── mod.rs        # register_tokenizers()
│   │           ├── jieba.rs      # JiebaTokenizer wrapper
│   │           ├── code.rs       # CodeTokenizer
│   │           └── filename.rs   # FilenameTokenizer
│   │
│   ├── rn-indexer/               # 索引器主服務
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── service.rs        # IndexerService 主控制器
│   │       ├── crawler.rs        # 目錄掃描器
│   │       ├── queue.rs          # TaskQueue (BinaryHeap + crossbeam)
│   │       ├── throttle.rs       # IoThrottle
│   │       └── workers/
│   │           ├── mod.rs
│   │           ├── extract.rs    # ExtractWorker
│   │           ├── normalize.rs  # NormalizeWorker
│   │           ├── index_writer.rs # IndexWriterWorker (串行 commit)
│   │           └── tombstone.rs  # TombstoneWorker
│   │
│   ├── rn-gpu/                   # GPU 加速 (feature-gated)
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs            # GpuBackend trait
│   │       ├── dispatcher.rs     # GpuDispatcher
│   │       ├── factory.rs        # create_best_available_backend()
│   │       ├── null_backend.rs   # CPU fallback
│   │       └── backends/
│   │           ├── cuda.rs       # CUDA backend
│   │           └── rocm.rs       # ROCm backend
│   │
│   ├── rn-windows/               # Windows 平台整合
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs
│   │       ├── service.rs        # Windows Service SCM
│   │       ├── install.rs        # Service install/uninstall
│   │       ├── watcher.rs        # FSWatcher (notify wrapper)
│   │       ├── debounce.rs       # Debouncer
│   │       └── reconcile.rs      # 定期校正
│   │
│   ├── rn-sdk/                   # Public API: Rust lib + C FFI + HTTP server
│   │   ├── Cargo.toml
│   │   └── src/
│   │       ├── lib.rs            # Rust API
│   │       ├── ffi.rs            # C ABI exports
│   │       └── http.rs           # axum HTTP server
│   │
│   ├── rn-gui/                   # Tauri 2.x 桌面應用
│   │   ├── Cargo.toml
│   │   ├── src/
│   │   │   └── main.rs
│   │   └── frontend/             # Web UI (HTML/CSS/JS)
│   │
│   └── rn-cli/                   # CLI 工具
│       ├── Cargo.toml
│       └── src/
│           ├── main.rs
│           └── commands/
│               ├── init.rs
│               ├── index.rs
│               ├── search.rs
│               ├── watch.rs
│               ├── doctor.rs
│               ├── stats.rs
│               ├── repair.rs
│               └── service.rs
│
├── tools/
│   ├── bench/                    # Criterion benchmarks
│   └── smoke/                    # End-to-end smoke test harness
│
├── installer/
│   ├── wix/                      # WiX 4 .wxs source (主要)
│   │   └── main.wxs
│   └── recoll-next.iss           # Inno Setup (備選)
│
├── docs/
│   ├── technical-design.md       # 本文件
│   ├── api-reference.md
│   └── user-guide.md
│
└── tests/                        # 整合測試
    ├── fixtures/
    │   ├── sample.pdf
    │   ├── sample.docx
    │   ├── sample.xlsx
    │   ├── sample.txt
    │   ├── sample.md
    │   ├── sample.html
    │   └── sample.rs
    ├── test_crawl.rs
    ├── test_extract.rs
    ├── test_search.rs
    └── test_watcher.rs
```

### 3.2 Crate 相依規則（強制單向）

```
rn-gui        → rn-sdk
rn-cli        → rn-sdk
rn-sdk        → rn-search, rn-meta, rn-core  (透過 IPC 與 rn-indexer 通訊)
rn-indexer    → rn-extractors, rn-search, rn-meta, rn-windows, rn-core
               (+ rn-gpu 透過 feature flag "gpu")
rn-search     → rn-core
rn-meta       → rn-core
rn-extractors → rn-core
rn-gpu        → rn-core
rn-windows    → rn-core
```

> **原則**：rn-core 不得相依任何其他 rn-* crate。rn-gpu 透過 `#[cfg(feature = "gpu")]` 封鎖，disable 時整個 crate 不參與編譯。

### 3.3 Workspace Cargo.toml

```toml
[workspace]
resolver = "2"
members = [
    "crates/rn-core",
    "crates/rn-indexer",
    "crates/rn-extractors",
    "crates/rn-search",
    "crates/rn-meta",
    "crates/rn-gpu",
    "crates/rn-windows",
    "crates/rn-sdk",
    "crates/rn-gui",
    "crates/rn-cli",
]

[workspace.dependencies]
tantivy = "0.22"
tokio = { version = "1", features = ["full"] }
rusqlite = { version = "0.31", features = ["bundled"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
toml = "0.8"
tracing = "0.1"
tracing-subscriber = { version = "0.3", features = ["env-filter", "json"] }
tracing-appender = "0.2"
anyhow = "1"
thiserror = "1"
uuid = { version = "1", features = ["v4", "serde"] }
crossbeam-channel = "0.5"
rayon = "1"
notify = "6"
walkdir = "2"
windows-service = "0.7"
clap = { version = "4", features = ["derive"] }
jieba-rs = "0.7"
pdf-extract = "0.7"
docx-rs = "0.4"
calamine = "0.24"
quick-xml = "0.31"
pulldown-cmark = "0.10"
unicode-normalization = "0.1"
unicode-segmentation = "1"
xxhash-rust = { version = "0.8", features = ["xxh3"] }
glob = "0.3"
axum = "0.7"
async-trait = "0.1"
chrono = { version = "0.4", features = ["serde"] }
mime_guess = "2"
tempfile = "3"
criterion = "0.5"
```

---

## 4. 核心資料模型 (rn-core)

### 4.1 IndexTask

```rust
// crates/rn-core/src/task.rs

use std::path::PathBuf;
use std::time::{Instant, SystemTime};
use uuid::Uuid;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IndexTask {
    pub task_id:      Uuid,
    pub file_path:    PathBuf,
    pub file_version: FileVersion,
    pub priority:     TaskPriority,
    pub operation:    OperationType,
    pub source:       TaskSource,
    pub scheduled_at: SystemTime,
    pub retry_count:  u8,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct FileVersion {
    pub mtime:  SystemTime,
    pub size:   u64,
    /// SHA-256，僅在 verify 模式或設定要求時計算
    pub hash:   Option<[u8; 32]>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
pub enum TaskPriority {
    High   = 0,
    Normal = 1,
    Low    = 2,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum OperationType {
    Create,
    Update,
    Delete,
    Verify,   // 僅比對 hash，不重建索引
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum TaskSource {
    InitialScan,
    Watcher,
    Manual,
    Retry,
    Reconcile,
}
```

### 4.2 DocumentState（狀態機）

```rust
// crates/rn-core/src/state.rs

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum DocumentState {
    Discovered,
    Queued,
    Extracting,
    Extracted,
    Normalized,
    Indexed,
    Failed { error: String, retry_count: u8 },
    Stale,
    Deleted,
}
```

**合法轉換表**：

| From | To | 觸發條件 |
|------|----|---------|
| Discovered | Queued | 排入 task queue |
| Queued | Extracting | ExtractWorker 取得 task |
| Extracting | Extracted | extract() 成功 |
| Extracted | Normalized | NormalizeWorker 完成 |
| Normalized | Indexed | Tantivy writer commit 成功 |
| 任何 | Failed | 任何階段拋出 error |
| Failed | Queued | retry_count < MAX_RETRY (預設 3) |
| Indexed | Stale | Watcher 偵測到 mtime/size 變更 |
| Stale | Queued | 重新排入 Update task |
| Indexed/Stale | Deleted | Watcher 偵測到刪除事件 |
| Deleted | (tombstone) | TombstoneWorker 清除 Tantivy + metadata |

### 4.3 ExtractResult

```rust
// crates/rn-core/src/extract.rs

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExtractResult {
    pub raw_text:           String,
    pub title:              Option<String>,
    pub summary_hint:       Option<String>,      // 前 512 字元，供 snippet 快取
    pub detected_language:  Option<Language>,
    pub page_count:         Option<u32>,
    pub sheet_names:        Vec<String>,          // for XLSX
    pub attachments:        Vec<AttachmentMeta>,
    pub warnings:           Vec<ExtractWarning>,
    pub extraction_time_ms: u64,
    pub extraction_method:  ExtractionMethod,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Language { En, ZhTw, ZhCn, Ja, Ko, Unknown }

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ExtractionMethod { Native, ExternalTool, Ocr, Fallback }

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AttachmentMeta {
    pub name:      String,
    pub mime_type: Option<String>,
    pub size:      Option<u64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ExtractWarning {
    PartialContent { reason: String },
    EncodingIssue   { chars_replaced: u32 },
    TruncatedAt     { bytes: u64 },
    OcrUsed,
}
```

### 4.4 SearchResult

```rust
// crates/rn-core/src/search.rs

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchResult {
    pub doc_id:        String,
    pub file_path:     PathBuf,
    pub filename:      String,
    pub title:         Option<String>,
    pub snippet:       String,    // highlighted HTML fragment
    pub score:         f32,
    pub match_reason:  MatchReason,
    pub modified_at:   SystemTime,
    pub file_size:     u64,
    pub mime_type:     String,
    pub source_type:   SourceType,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum MatchReason {
    FilenameExact,
    FilenamePrefix,
    ContentPhrase,
    ContentKeyword,
    TitleMatch,
    Combined { filename_score: f32, content_score: f32 },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SourceType { File, Email, Code, Archive, Unknown }

pub struct SearchResponse {
    pub results:        Vec<SearchResult>,
    pub total_hits:     usize,
    pub metadata_hits:  usize,
    pub fulltext_hits:  usize,
    pub duration:       Duration,
}
```

---

## 5. Indexer Pipeline (rn-indexer)

### 5.1 整體 Pipeline 架構

```
┌─────────────────────────────────────────────────────────┐
│                    IndexerService                        │
│                                                         │
│  ┌──────────┐    ┌──────────┐    ┌────────────────────┐ │
│  │ FSWatcher│───▶│TaskQueue │───▶│  Worker Pool       │ │
│  └──────────┘    │(priority)│    │                    │ │
│  ┌──────────┐    └──────────┘    │ DiscoveryWorker x2 │ │
│  │ Crawler  │───▶              ▶ │ ExtractWorker   x4 │ │
│  └──────────┘                    │ NormalizeWorker x4 │ │
│                                  │ GpuDispatcher   x1 │ │
│                                  │ IndexWriter     x1 │ │
│                                  │ TombstoneWorker x1 │ │
│                                  └────────────────────┘ │
│                                           │              │
│                         ┌─────────────────┴──────────┐  │
│                         │  Tantivy Index  │  SQLite   │  │
│                         └────────────────┴───────────┘  │
└─────────────────────────────────────────────────────────┘
```

### 5.2 IndexerService 主結構

```rust
// crates/rn-indexer/src/service.rs

use std::sync::Arc;
use tokio::sync::RwLock;

pub struct IndexerService {
    config:         Arc<IndexerConfig>,
    task_queue:     Arc<TaskQueue>,
    meta_store:     Arc<MetaStore>,          // rn-meta
    search_engine:  Arc<SearchEngine>,       // rn-search
    gpu_dispatcher: Option<Arc<GpuDispatcher>>, // rn-gpu，None = CPU-only
    watcher:        Arc<FsWatcher>,          // rn-windows / notify
    state:          Arc<RwLock<ServiceState>>,
    shutdown:       tokio::sync::CancellationToken,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ServiceState {
    Starting,
    InitialCrawl { progress: CrawlProgress },
    Watching,
    Paused,
    ShuttingDown,
}

impl IndexerService {
    pub async fn start(config: IndexerConfig) -> anyhow::Result<Self> { ... }
    pub async fn pause(&self) -> anyhow::Result<()> { ... }
    pub async fn resume(&self) -> anyhow::Result<()> { ... }
    pub async fn reindex_path(&self, path: &Path) -> anyhow::Result<()> { ... }
    pub async fn shutdown(self) -> anyhow::Result<()> { ... }
    pub async fn health(&self) -> HealthReport { ... }
}
```

### 5.3 TaskQueue

採用 **BinaryHeap + crossbeam-channel** 組合：priority queue 保證 High 任務優先，channel 解耦生產者與消費者。

```rust
// crates/rn-indexer/src/queue.rs

use std::collections::BinaryHeap;
use std::sync::{Arc, Mutex};
use crossbeam_channel::{Receiver, Sender};

pub struct TaskQueue {
    inner:    Arc<Mutex<BinaryHeap<PrioritizedTask>>>,
    notify_tx: Sender<()>,
    notify_rx: Receiver<()>,
    metrics:  Arc<QueueMetrics>,
}

/// BinaryHeap 需要 Ord；以 (priority, scheduled_at.elapsed()) 排序
#[derive(Debug, Eq, PartialEq)]
struct PrioritizedTask {
    priority:    TaskPriority,
    received_at: std::time::Instant,
    task:        IndexTask,
}

impl Ord for PrioritizedTask {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        // 先比 priority（數字小 = 高優先），再比 received_at（早的優先）
        other.priority.cmp(&self.priority)
            .then(self.received_at.cmp(&other.received_at))
    }
}

impl PartialOrd for PrioritizedTask {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl TaskQueue {
    pub fn push(&self, task: IndexTask) { ... }
    pub fn pop_blocking(&self) -> IndexTask { ... }  // workers call this
    pub fn len(&self) -> usize { ... }
    pub fn is_empty(&self) -> bool { ... }
}
```

### 5.4 Worker Pool 設計

每種 worker 是獨立的 Tokio task，透過共享 Arc 存取資源。

#### 5.4.1 ExtractWorker

```rust
// crates/rn-indexer/src/workers/extract.rs

pub struct ExtractWorker {
    id:           usize,
    queue:        Arc<TaskQueue>,
    extractors:   Arc<ExtractorRegistry>,
    meta_store:   Arc<MetaStore>,
    output_tx:    Sender<ExtractedDoc>,
    config:       Arc<WorkerConfig>,
    shutdown:     CancellationToken,
}

pub struct ExtractedDoc {
    pub task:   IndexTask,
    pub result: ExtractResult,
}

impl ExtractWorker {
    pub async fn run(mut self) {
        loop {
            tokio::select! {
                _ = self.shutdown.cancelled() => break,
                task = self.pop_task() => {
                    self.meta_store.set_state(&task.file_path, DocumentState::Extracting).await?;
                    match self.extractors.extract(&task).await {
                        Ok(result) => {
                            self.meta_store.set_state(&task.file_path, DocumentState::Extracted).await?;
                            self.output_tx.send(ExtractedDoc { task, result })?;
                        }
                        Err(e) => self.handle_failure(&task, e).await,
                    }
                }
            }
        }
    }

    async fn handle_failure(&self, task: &IndexTask, e: anyhow::Error) {
        let new_retry = task.retry_count + 1;
        if new_retry < self.config.max_retry {
            let backoff = Duration::from_secs(2u64.pow(new_retry as u32));
            let retry_task = IndexTask { retry_count: new_retry, ..task.clone() };
            tokio::time::sleep(backoff).await;
            self.queue.push(retry_task);
        } else {
            self.meta_store.set_state(&task.file_path,
                DocumentState::Failed { error: e.to_string(), retry_count: new_retry }
            ).await.ok();
            tracing::error!(path = ?task.file_path, error = %e, "max retries exceeded");
        }
    }
}
```

#### 5.4.2 NormalizeWorker

```rust
// crates/rn-indexer/src/workers/normalize.rs

pub struct NormalizeWorker {
    id:        usize,
    input_rx:  Receiver<ExtractedDoc>,
    output_tx: Sender<NormalizedDoc>,
    tokenizer: Arc<TokenizerPipeline>,
    config:    Arc<WorkerConfig>,
    shutdown:  CancellationToken,
}

pub struct NormalizedDoc {
    pub task:            IndexTask,
    pub tantivy_doc:     tantivy::Document,
    pub metadata_record: MetadataRecord,
}

impl NormalizeWorker {
    fn process(&self, doc: ExtractedDoc) -> anyhow::Result<NormalizedDoc> {
        // 1. Unicode NFC normalization
        let text = unicode_normalization::UnicodeNormalization::nfc(&doc.result.raw_text)
            .collect::<String>();

        // 2. 語言偵測（必要時）
        let lang = doc.result.detected_language
            .unwrap_or_else(|| detect_language(&text));

        // 3. 截斷超大文件（預設 MAX_CONTENT_BYTES = 10 MB）
        let text = truncate_to_bytes(text, self.config.max_content_bytes);

        // 4. 建立 Tantivy Document
        let tantivy_doc = build_tantivy_doc(&doc.task, &doc.result, &text, lang)?;

        // 5. 建立 MetadataRecord
        let metadata_record = build_metadata_record(&doc.task)?;

        Ok(NormalizedDoc { task: doc.task, tantivy_doc, metadata_record })
    }
}
```

#### 5.4.3 IndexWriterWorker（單一 writer，串行 commit）

Tantivy 的 `IndexWriter` 不是 Sync，**全系統只允許一個 writer instance**，所有 commit 都透過此 worker 串行處理。

```rust
// crates/rn-indexer/src/workers/index_writer.rs

pub struct IndexWriterWorker {
    writer:       tantivy::IndexWriter,
    input_rx:     Receiver<NormalizedDoc>,
    meta_store:   Arc<MetaStore>,
    commit_policy: CommitPolicy,
    pending:      usize,
    last_commit:  Instant,
    shutdown:     CancellationToken,
}

pub enum CommitPolicy {
    /// 每 N 個文件 commit 一次（throughput mode）
    ByCount { n: usize },
    /// 距上次 commit 超過 T 秒則 commit（latency mode）
    ByTime  { secs: u64 },
    /// 混合：N 文件或 T 秒，先到先 commit（balanced mode）
    Hybrid  { n: usize, secs: u64 },
}

impl IndexWriterWorker {
    pub async fn run(mut self) {
        loop {
            tokio::select! {
                _ = self.shutdown.cancelled() => {
                    self.do_commit().await.ok();
                    break;
                }
                maybe_doc = recv_async(&self.input_rx) => {
                    match maybe_doc {
                        Ok(doc) => {
                            self.writer.add_document(doc.tantivy_doc).unwrap();
                            self.meta_store.upsert_metadata(&doc.metadata_record).await.ok();
                            self.meta_store.set_state(
                                &doc.task.file_path, DocumentState::Indexed
                            ).await.ok();
                            self.pending += 1;
                            if self.should_commit() {
                                self.do_commit().await.ok();
                            }
                        }
                        Err(_) => break,
                    }
                }
            }
        }
    }

    fn should_commit(&self) -> bool {
        match self.commit_policy {
            CommitPolicy::ByCount { n } => self.pending >= n,
            CommitPolicy::ByTime  { secs } => self.last_commit.elapsed().as_secs() >= secs,
            CommitPolicy::Hybrid  { n, secs } =>
                self.pending >= n || self.last_commit.elapsed().as_secs() >= secs,
        }
    }

    async fn do_commit(&mut self) -> anyhow::Result<()> {
        if self.pending == 0 { return Ok(()); }
        self.writer.commit()?;
        self.pending = 0;
        self.last_commit = Instant::now();
        tracing::info!("tantivy commit ok");
        Ok(())
    }
}
```

#### 5.4.4 TombstoneWorker

```rust
// crates/rn-indexer/src/workers/tombstone.rs

pub struct TombstoneWorker {
    input_rx:    Receiver<DeleteTask>,
    writer:      Arc<Mutex<tantivy::IndexWriter>>,
    meta_store:  Arc<MetaStore>,
    shutdown:    CancellationToken,
}

pub struct DeleteTask {
    pub file_path: PathBuf,
    pub doc_id:    Option<String>,
}

impl TombstoneWorker {
    async fn delete(&self, task: DeleteTask) -> anyhow::Result<()> {
        let doc_id = match task.doc_id {
            Some(id) => id,
            None => self.meta_store.get_doc_id(&task.file_path).await?
                        .ok_or_else(|| anyhow::anyhow!("doc_id not found"))?,
        };
        let term = tantivy::Term::from_field_text(DOC_ID_FIELD, &doc_id);
        self.writer.lock().unwrap().delete_term(term);
        self.meta_store.delete_by_path(&task.file_path).await?;
        tracing::info!(path = ?task.file_path, "tombstone ok");
        Ok(())
    }
}
```

### 5.5 初始掃描 Crawler

```rust
// crates/rn-indexer/src/crawler.rs

pub struct Crawler {
    config:     Arc<CrawlerConfig>,
    queue:      Arc<TaskQueue>,
    meta_store: Arc<MetaStore>,
    throttle:   Arc<IoThrottle>,
}

pub struct CrawlerConfig {
    pub root_paths:       Vec<PathBuf>,
    pub exclude_patterns: Vec<glob::Pattern>,
    pub max_file_size:    u64,              // bytes, 預設 100 MB
    pub follow_symlinks:  bool,
    pub include_hidden:   bool,
}

impl Crawler {
    /// 採用 tokio::task::spawn_blocking + rayon 並行走訪目錄樹
    pub async fn crawl(&self) -> anyhow::Result<CrawlStats> {
        let walker = walkdir::WalkDir::new(&self.config.root_paths[0])
            .follow_links(self.config.follow_symlinks)
            .into_iter()
            .filter_entry(|e| !self.is_excluded(e));

        let mut stats = CrawlStats::default();
        for entry in walker.flatten() {
            if entry.file_type().is_file() {
                let metadata = entry.metadata()?;
                if metadata.len() > self.config.max_file_size { continue; }

                let path = entry.path().to_owned();
                if self.meta_store.is_up_to_date(&path, &metadata).await? {
                    stats.skipped += 1;
                    continue;
                }

                self.queue.push(IndexTask {
                    task_id:      Uuid::new_v4(),
                    file_path:    path,
                    file_version: FileVersion::from_metadata(&metadata),
                    priority:     TaskPriority::Low,
                    operation:    OperationType::Create,
                    source:       TaskSource::InitialScan,
                    scheduled_at: SystemTime::now(),
                    retry_count:  0,
                });
                stats.queued += 1;
            }
            if stats.queued % 1000 == 0 {
                self.throttle.yield_if_needed().await;
            }
        }
        Ok(stats)
    }

    fn is_excluded(&self, entry: &walkdir::DirEntry) -> bool {
        self.config.exclude_patterns.iter().any(|p| p.matches_path(entry.path()))
    }
}
```

### 5.6 IO Throttle

```rust
// crates/rn-indexer/src/throttle.rs

pub struct IoThrottle {
    mode: ThrottleMode,
}

pub enum ThrottleMode {
    Off,
    Gentle { files_per_batch: u32, sleep_ms: u64 },
    CpuCap { max_percent: u8 },
}

impl IoThrottle {
    pub async fn yield_if_needed(&self) {
        match self.mode {
            ThrottleMode::Off => {}
            ThrottleMode::Gentle { sleep_ms, .. } => {
                tokio::time::sleep(Duration::from_millis(sleep_ms)).await;
            }
            ThrottleMode::CpuCap { .. } => {
                // 量測自上次 yield 的 elapsed，若過快則 sleep
            }
        }
    }
}
```

### 5.7 一致性保證

| 操作 | MetaDB | Tantivy | 保證 |
|------|--------|---------|------|
| 新增檔案 | INSERT (Discovered) | - | MetaDB 先寫 |
| 抽取完成 | UPDATE (Indexed) | add_document + commit | 先寫 Tantivy，成功後更新 MetaDB |
| 刪除檔案 | DELETE | delete_term(doc_id) + commit | 先刪 Tantivy，成功後刪 MetaDB |
| 更新檔案 | UPDATE (Queued) | delete + add | 視為 delete + create |
| Crash 後 | 掃描 state=Extracting/Normalized | 不處理 | 重新排入佇列 |

---

## 6. Tantivy Schema + 查詢設計 (rn-search)

### 6.1 Schema 定義

```rust
// crates/rn-search/src/schema.rs

use tantivy::schema::*;

pub struct RnSchema {
    pub schema:       Schema,
    pub doc_id:       Field,   // STRING | STORED
    pub path:         Field,   // STRING | STORED
    pub filename:     Field,   // TEXT(rn_filename) | STORED
    pub title:        Field,   // TEXT(rn_default) | STORED
    pub extension:    Field,   // STRING | STORED | FAST
    pub mime_type:    Field,   // STRING | STORED
    pub content:      Field,   // TEXT(rn_default) — NOT stored（節省磁碟）
    pub summary:      Field,   // STORED only（前 512 字元，供 snippet）
    pub language:     Field,   // STRING | STORED
    pub created_at:   Field,   // I64 | FAST + STORED
    pub modified_at:  Field,   // I64 | FAST + STORED
    pub indexed_at:   Field,   // I64 | FAST + STORED
    pub size_bytes:   Field,   // U64 | FAST + STORED
    pub source_type:  Field,   // STRING | STORED
    pub tags:         Field,   // TEXT | STORED
    pub checksum:     Field,   // STRING | STORED
}

impl RnSchema {
    pub fn build() -> Self {
        let mut builder = SchemaBuilder::default();

        let text_opt_stored = TextOptions::default()
            .set_indexing_options(
                TextFieldIndexing::default()
                    .set_tokenizer("rn_default")
                    .set_index_option(IndexRecordOption::WithFreqsAndPositions),
            )
            .set_stored();

        let text_opt_nostored = TextOptions::default()
            .set_indexing_options(
                TextFieldIndexing::default()
                    .set_tokenizer("rn_default")
                    .set_index_option(IndexRecordOption::WithFreqsAndPositions),
            );

        let i64_fast = NumericOptions::default().set_fast().set_stored();
        let u64_fast = NumericOptions::default().set_fast().set_stored();

        Self {
            doc_id:      builder.add_text_field("doc_id",      STRING | STORED),
            path:        builder.add_text_field("path",        STRING | STORED),
            filename:    builder.add_text_field("filename",    text_opt_stored.clone()),
            title:       builder.add_text_field("title",       text_opt_stored.clone()),
            extension:   builder.add_text_field("extension",   STRING | STORED | FAST),
            mime_type:   builder.add_text_field("mime_type",   STRING | STORED),
            content:     builder.add_text_field("content",     text_opt_nostored),
            summary:     builder.add_text_field("summary",     STORED),
            language:    builder.add_text_field("language",    STRING | STORED),
            created_at:  builder.add_i64_field("created_at",  i64_fast.clone()),
            modified_at: builder.add_i64_field("modified_at", i64_fast.clone()),
            indexed_at:  builder.add_i64_field("indexed_at",  i64_fast),
            size_bytes:  builder.add_u64_field("size_bytes",  u64_fast),
            source_type: builder.add_text_field("source_type",STRING | STORED),
            tags:        builder.add_text_field("tags",        text_opt_stored),
            checksum:    builder.add_text_field("checksum",   STRING | STORED),
            schema:      builder.build(),
        }
    }
}
```

### 6.2 Tokenizer Pipeline

```rust
// crates/rn-search/src/tokenizer/mod.rs

pub fn register_tokenizers(index: &tantivy::Index) {
    let manager = index.tokenizers();

    // 英文 / 通用文字
    manager.register("rn_default",
        TextAnalyzer::builder(SimpleTokenizer::default())
            .filter(LowerCaser)
            .filter(StopWordFilter::remove(DEFAULT_STOPWORDS.iter().copied()))
            .filter(Stemmer::new(Language::English))
            .build(),
    );

    // CJK：jieba 分詞
    manager.register("rn_cjk",
        TextAnalyzer::builder(JiebaTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );

    // Source code
    manager.register("rn_code",
        TextAnalyzer::builder(CodeTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );

    // Filename
    manager.register("rn_filename",
        TextAnalyzer::builder(FilenameTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );
}
```

### 6.3 Snippet 生成

```rust
// crates/rn-search/src/snippet.rs

pub struct SnippetBuilder {
    max_fragment_chars: usize,   // 預設 200
    highlight_open:     String,  // "<em>"
    highlight_close:    String,  // "</em>"
}

impl SnippetBuilder {
    pub fn generate(
        &self,
        searcher: &tantivy::Searcher,
        query:    &dyn tantivy::query::Query,
        doc:      &tantivy::Document,
        field:    Field,
    ) -> String {
        let gen = SnippetGenerator::create(searcher, query, field)
            .expect("snippet generator");
        gen.snippet_from_doc(doc).to_html()
    }
}
```

---

## 7. Metadata Index (rn-meta)

### 7.1 SQLite Schema

```sql
-- crates/rn-meta/migrations/001_init.sql

PRAGMA journal_mode = WAL;
PRAGMA synchronous  = NORMAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS files (
    file_id       TEXT PRIMARY KEY,   -- UUID
    path          TEXT NOT NULL UNIQUE,
    filename      TEXT NOT NULL,
    extension     TEXT,
    volume_id     TEXT,               -- Windows: \\?\Volume{...}
    size          INTEGER NOT NULL,
    created_at    INTEGER,            -- Unix timestamp (seconds)
    modified_at   INTEGER NOT NULL,
    accessed_at   INTEGER,
    file_hash     BLOB,               -- SHA-256, nullable
    mime_type     TEXT,
    is_directory  INTEGER NOT NULL DEFAULT 0,
    index_state   TEXT NOT NULL DEFAULT 'discovered',
    doc_id        TEXT,               -- Tantivy doc_id，indexed 後填入
    retry_count   INTEGER NOT NULL DEFAULT 0,
    last_error    TEXT,
    indexed_at    INTEGER,
    updated_at    INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE INDEX IF NOT EXISTS idx_files_path          ON files(path);
CREATE INDEX IF NOT EXISTS idx_files_extension     ON files(extension);
CREATE INDEX IF NOT EXISTS idx_files_modified_at   ON files(modified_at DESC);
CREATE INDEX IF NOT EXISTS idx_files_index_state   ON files(index_state);
CREATE INDEX IF NOT EXISTS idx_files_filename      ON files(filename);
CREATE INDEX IF NOT EXISTS idx_files_filename_lower ON files(lower(filename));

CREATE TABLE IF NOT EXISTS watch_roots (
    root_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    path        TEXT NOT NULL UNIQUE,
    enabled     INTEGER NOT NULL DEFAULT 1,
    added_at    INTEGER NOT NULL DEFAULT (unixepoch())
);

CREATE TABLE IF NOT EXISTS exclude_rules (
    rule_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    pattern     TEXT NOT NULL,
    rule_type   TEXT NOT NULL DEFAULT 'glob',
    enabled     INTEGER NOT NULL DEFAULT 1
);

CREATE TABLE IF NOT EXISTS index_stats (
    stat_key    TEXT PRIMARY KEY,
    stat_value  TEXT NOT NULL,
    updated_at  INTEGER NOT NULL DEFAULT (unixepoch())
);
```

### 7.2 MetaStore 介面

```rust
// crates/rn-meta/src/store.rs

pub struct MetaStore {
    conn: Arc<Mutex<Connection>>,
}

impl MetaStore {
    pub fn open(db_path: &Path) -> anyhow::Result<Self>;
    pub async fn is_up_to_date(&self, path: &Path, meta: &std::fs::Metadata) -> anyhow::Result<bool>;
    pub async fn upsert_metadata(&self, record: &MetadataRecord) -> anyhow::Result<()>;
    pub async fn search_filename_prefix(&self, prefix: &str, limit: usize) -> anyhow::Result<Vec<FileMeta>>;
    pub async fn set_state(&self, path: &Path, state: DocumentState) -> anyhow::Result<()>;
    pub async fn get_doc_id(&self, path: &Path) -> anyhow::Result<Option<String>>;
    pub async fn delete_by_path(&self, path: &Path) -> anyhow::Result<()>;
    pub async fn get_failed_tasks(&self, limit: usize) -> anyhow::Result<Vec<IndexTask>>;
    pub async fn find_stale(&self, root: &Path) -> anyhow::Result<Vec<FileMeta>>;
    pub async fn get_stats(&self) -> anyhow::Result<IndexStats>;
}
```

---

## 8. 文件內容抽取器 (rn-extractors)

### 8.1 Extractor Trait

```rust
// crates/rn-extractors/src/lib.rs

#[async_trait]
pub trait Extractor: Send + Sync {
    fn supports(&self, mime: &str, extension: &str) -> bool;
    async fn extract(&self, path: &Path) -> anyhow::Result<ExtractResult>;
    async fn extract_metadata(&self, path: &Path) -> anyhow::Result<FileMeta>;
    fn cost_estimate(&self, size: u64) -> CostProfile;
    fn name(&self) -> &'static str;
}

#[derive(Debug, Clone)]
pub struct CostProfile {
    pub cpu_intensity:    CostLevel,
    pub memory_estimate:  u64,
    pub gpu_offloadable:  bool,
}

#[derive(Debug, Clone, Copy)]
pub enum CostLevel { Low, Medium, High }
```

### 8.2 ExtractorRegistry

```rust
// crates/rn-extractors/src/registry.rs

pub struct ExtractorRegistry {
    extractors: Vec<Box<dyn Extractor>>,
}

impl ExtractorRegistry {
    pub fn default_registry() -> Self {
        Self {
            extractors: vec![
                Box::new(PlainTextExtractor::new()),
                Box::new(PdfExtractor::new()),
                Box::new(DocxExtractor::new()),
                Box::new(XlsxExtractor::new()),
                Box::new(PptxExtractor::new()),
                Box::new(HtmlExtractor::new()),
                Box::new(MarkdownExtractor::new()),
                Box::new(SourceCodeExtractor::new()),
                Box::new(EmlExtractor::new()),
                Box::new(CsvExtractor::new()),
                Box::new(FallbackExtractor::new()),
            ],
        }
    }

    pub async fn extract(&self, task: &IndexTask) -> anyhow::Result<ExtractResult> {
        let ext  = task.file_path.extension()
            .and_then(|e| e.to_str()).unwrap_or("");
        let mime = mime_guess::from_path(&task.file_path)
            .first_or_octet_stream().to_string();

        let extractor = self.extractors.iter()
            .find(|e| e.supports(&mime, ext))
            .ok_or_else(|| anyhow::anyhow!("no extractor for {}", task.file_path.display()))?;

        extractor.extract(&task.file_path).await
    }
}
```

---

## 9. 檔案監控 (rn-windows / notify)

### 9.1 FSWatcher

```rust
// crates/rn-windows/src/watcher.rs

pub struct FsWatcher {
    _watcher:    RecommendedWatcher,
    event_tx:    Sender<WatchEvent>,
    debouncer:   Arc<Debouncer>,
}

#[derive(Debug, Clone)]
pub struct WatchEvent {
    pub kind:  WatchEventKind,
    pub path:  PathBuf,
    pub ts:    Instant,
}

#[derive(Debug, Clone, Copy)]
pub enum WatchEventKind { Created, Modified, Renamed, Deleted }
```

### 9.2 Debouncer

```rust
// crates/rn-windows/src/debounce.rs

pub struct Debouncer {
    window_ms: u64,
    pending:   Mutex<HashMap<PathBuf, Instant>>,
}

impl Debouncer {
    pub fn should_emit(&self, path: &PathBuf) -> bool { ... }
}
```

### 9.3 定期校正 (Reconciler)

```rust
// crates/rn-windows/src/reconcile.rs

pub struct Reconciler {
    interval: Duration,  // 預設 1 小時
}

pub struct ReconcileReport {
    pub new_files:      Vec<PathBuf>,
    pub modified_files: Vec<PathBuf>,
    pub deleted_files:  Vec<String>,
    pub unchanged:      usize,
    pub duration:       Duration,
}
```

---

## 10. 查詢與排序

### 10.1 QueryParser

支援語法：關鍵字、精確詞組、布林、欄位限定、日期範圍、路徑前綴、大小過濾、萬用字元前綴。

```rust
// crates/rn-search/src/query.rs

pub struct QueryParser {
    schema:          RnSchema,
    default_fields:  Vec<Field>,    // [filename, title, content]
    tantivy_parser:  tantivy::query::QueryParser,
}

impl QueryParser {
    pub fn parse(&self, input: &str) -> anyhow::Result<Box<dyn Query>>;
}
```

### 10.2 混合排序

```rust
// crates/rn-search/src/ranking.rs

pub struct RankingWeights {
    pub fulltext:           f32,   // 0.60
    pub filename:           f32,   // 0.30
    pub recency:            f32,   // 0.10
    pub semantic:           f32,   // 0.00（第二期）
    pub exact_phrase_boost: f32,   // 2.0
    pub title_match_boost:  f32,   // 1.5
}

pub fn compute_filename_score(query_tokens: &[&str], filename: &str) -> f32;
pub fn compute_recency_score(modified_at: i64) -> f32;
```

### 10.3 查詢協調器

```rust
// crates/rn-search/src/reader.rs

pub struct SearchCoordinator {
    meta_store:     Arc<MetaStore>,
    tantivy_reader: IndexReader,
    schema:         RnSchema,
    ranking:        RankingWeights,
}

impl SearchCoordinator {
    pub async fn search(&self, request: SearchRequest) -> anyhow::Result<SearchResponse> {
        // 並行查詢 MetaDB + Tantivy，合併排序，分頁
    }
}
```

---

## 11. GPU 加速架構 (rn-gpu)

### 11.1 設計原則

- `rn-gpu` 透過 Cargo feature `gpu` 啟用，disable 時不參與編譯
- GPU 任務採 batch 模式，min_batch_size = 32
- 小文件（< 10KB）不 dispatch 到 GPU

### 11.2 GpuBackend Trait

```rust
#[async_trait]
pub trait GpuBackend: Send + Sync {
    async fn batch_preprocess(&self, texts: Vec<String>) -> anyhow::Result<Vec<PreprocessResult>>;
    async fn batch_embed(&self, texts: Vec<String>) -> anyhow::Result<Vec<Embedding>>;
    fn device_info(&self) -> GpuDeviceInfo;
    fn is_available(&self) -> bool;
}

#[derive(Debug, Clone, Copy)]
pub enum GpuBackendType { Cuda, Rocm, DirectMl, Vulkan, None }
```

### 11.3 Backend 選擇：CUDA → ROCm → NullBackend (CPU fallback)

---

## 12. Windows 平台整合 (rn-windows)

### 12.1 Windows Service

- Service name: `RecollNextIndexer`
- 以 `windows-service` crate 整合 SCM
- 支援 install / uninstall / start / stop

### 12.2 Windows Explorer 整合

透過 Registry 註冊右鍵選單 Shell Extension。

### 12.3 安裝器 (WiX 4 MSI)

安裝流程：複製檔案 → 註冊 Shell Extension → 安裝 Service（可選）→ 加入 PATH → 建立捷徑

---

## 13. GUI 設計 (rn-gui)

### 13.1 技術：Tauri 2.x

Web 技術 UI + Rust 後端 + 系統 WebView

### 13.2 畫面規格

```
┌─────────────────────────────────────────────┐
│  [搜尋框]  [篩選] type/path/date  [設定]     │
├─────────────────────────┬───────────────────┤
│  搜尋結果列表            │  預覽面板         │
├─────────────────────────┴───────────────────┤
│  狀態列: 已索引 N 檔 | 佇列 N | GPU 狀態    │
└─────────────────────────────────────────────┘
```

---

## 14. CLI 設計 (rn-cli)

```
rn-cli init | index | search | watch | doctor | stats | repair | gpu | service | config

rn-cli search "cuda v100" --limit 3
Found 42 results (18ms)
 1. [PDF]  cuda_v100_whitepaper.pdf    Score: 0.95
    C:\docs\nvidia\cuda_v100_whitepaper.pdf
    "...the CUDA architecture on V100 provides 5120 cores..."
    Modified: 2025-01-15  Size: 2.3 MB
```

---

## 15. SDK / Public API (rn-sdk)

- **Rust API**: `RecollNext::open()`, `search()`, `index_path()`, `stats()`, `health()`
- **C FFI**: `rn_open()`, `rn_search()`, `rn_close()`
- **HTTP API**: axum server on `127.0.0.1:9312`

---

## 16. 設定系統

### 16.1 設定檔：`%APPDATA%\RecollNext\config.toml`

```toml
[indexer]
root_paths = ["C:\\Users\\username\\Documents"]
exclude_patterns = ["**\\.git\\**", "**\\node_modules\\**"]
max_file_size_mb = 100

[workers]
extract_workers = 4
pause_on_battery = true

[commit]
policy = "hybrid"
count = 500
time_secs = 30

[gpu]
mode = "auto"

[search]
ranking_fulltext = 0.60
ranking_filename = 0.30
ranking_recency  = 0.10

[windows]
register_context_menu = true
start_with_windows = true
show_tray_icon = true
```

### 16.2 載入優先序

CLI args > 環境變數 RECOLL_* > user config > system config > defaults

---

## 17. 日誌與監控

- tracing + tracing-subscriber + tracing-appender
- 結構化 JSON 日誌，DAILY rotation
- 健康檢查：metadata_db / tantivy / watcher / gpu 各元件狀態

---

## 18. 錯誤處理與恢復

### 18.1 錯誤分層

| 層次 | 型別 |
|------|------|
| 函式庫 crate | `thiserror` enum (`IndexError`) |
| 應用層 / workers | `anyhow::Error` |
| FFI | `i32` error code |
| HTTP API | JSON `{ "error": "..." }` |

### 18.2 Panic Recovery

Worker panic → `catch_unwind` → 5s 後自動重啟

### 18.3 Crash Recovery

啟動 → 檢查 lock file → PRAGMA integrity_check → 重設 Extracting/Normalized 狀態 → 正常啟動

### 18.4 Repair

`rn-cli repair` 修復 orphan entries、state 不一致、Tantivy segment merge

---

## 19. 安全性

- HTTP API 僅綁定 `127.0.0.1`
- 日誌不記錄全文，snippet 長度受限
- Exclude 規則排除敏感資料夾
- `rn-cli repair --purge` 清空索引

---

## 20. 測試策略

### 20.1 測試分層

| Layer | Target | 工具 |
|-------|--------|------|
| Unit Tests | > 80% | cargo test |
| Integration | 關鍵路徑 | tests/ + fixtures |
| Smoke Tests | E2E | tools/smoke/ |
| Benchmark | 效能基線 | tools/bench/ (Criterion) |

### 20.2 單元測試範圍

```
rn-core:        TaskPriority 排序、DocumentState 轉換
rn-search:      QueryParser 語法、scoring、snippet
rn-meta:        upsert、prefix search、is_up_to_date
rn-extractors:  supports() 判斷、fixture 檔案抽取
rn-gpu:         NullBackend fallback、batch flush
rn-windows:     Debouncer、event mapping
```

---

## 21. CI/CD Pipeline

### 21.1 CI：push/PR → fmt + clippy + test (Windows + Linux)

### 21.2 Release：tag v* → build release → test → WiX MSI → upload artifacts

---

## 22. 效能目標與基準測試

| 指標 | 目標值 |
|------|--------|
| 檔名前綴搜尋 (1M 檔) | < 30 ms (p99) |
| 全文查詢 (10K 文件) | < 300 ms (p99) |
| MetaDB 寫入吞吐 | > 5,000 records/sec |
| 初次索引吞吐 | > 500 files/sec (SSD) |
| Tantivy commit | < 200 ms (500 docs) |
| 記憶體使用 | < 500 MB (100K 索引) |
| 磁碟使用 | 原始資料 10-30% |
| 背景 CPU | < 60%（可設定） |

---

## 23. Phase 0 PoC 工作清單

### Week 1-2（基礎建設）

- [ ] 建立 Cargo Workspace，`cargo build` 全 crate 可過
- [ ] `rn-core`：IndexTask, DocumentState, ExtractResult, SearchResult
- [ ] `rn-meta`：SQLite schema + MetaStore CRUD
- [ ] `rn-meta`：單元測試

### Week 3-4（Tantivy PoC）

- [ ] `rn-search`：RnSchema + 基本讀寫
- [ ] `rn-search`：JiebaTokenizer + 中文分詞驗證
- [ ] `rn-search`：QueryParser keyword + field 查詢
- [ ] `rn-search`：10 萬筆 benchmark < 300 ms

### Week 5-6（Indexer Pipeline PoC）

- [ ] `rn-extractors`：PlainText + PDF + DOCX
- [ ] `rn-indexer`：TaskQueue + ExtractWorker + IndexWriterWorker pipeline
- [ ] `rn-indexer`：Crawler 掃描 1 萬筆
- [ ] Smoke test：E2E 索引 + 搜尋

### Week 7-8（Windows + GPU PoC）

- [ ] `rn-windows`：Windows Service install/start/stop
- [ ] `rn-windows`：FsWatcher + Debouncer
- [ ] `rn-gpu`：NullBackend + CudaBackend::try_init()
- [ ] CI：GitHub Actions Windows build + MSI

### Phase 0 交付物

1. `rn-cli index` + `rn-cli search` MVP
2. `benchmark_report_phase0.md`
3. `risk_report_phase0.md`

---

## 24. 里程碑與交付物

| Phase | 名稱 | 週期 | 交付物 |
|-------|------|------|--------|
| 0 | 研究與 PoC | 8 週 | 技術驗證 + benchmark + 風險清單 |
| 1 | 核心索引器 MVP | 8-12 週 | CLI：crawl + index + search + repair |
| 2 | Windows 正式版 | 8-10 週 | GUI + installer + service + watcher |
| 3 | GPU + 混合排序 | 8-12 週 | GPU preprocess + ranking + benchmark |
| 4 | 穩定化發行 | 6-8 週 | v1.0 + 文件 + SDK |

---

## 25. 風險與緩解

| # | 風險 | 影響 | 機率 | 緩解 |
|---|------|------|------|------|
| R1 | Tantivy CJK tokenizer 品質不足 | 中文搜尋差 | 中 | jieba-rs 自訂 tokenizer |
| R2 | Windows watcher 漏事件 | 索引不一致 | 高 | 必須 reconcile scan |
| R3 | PDF 抽取相依過重 | 打包複雜 | 中 | pdf-extract (pure Rust) + fallback |
| R4 | GPU transfer overhead | 加速無意義 | 中 | min batch size + CPU fallback |
| R5 | Tantivy segment merge 卡頓 | 搜尋變慢 | 低 | 獨立 writer + commit policy |
| R6 | 磁碟 IO 過高 | 體驗差 | 高 | gentle throttle + pause |
| R7 | 索引 migration 失敗 | 版本斷裂 | 中 | schema_version + repair |

---

## 26. 附錄：HTTP API 參考

### 端點（Phase 2+）

```
GET  /api/v1/search?q=<query>&limit=20&offset=0
GET  /api/v1/stats
GET  /api/v1/health
POST /api/v1/index          { "paths": ["C:\\docs"] }
POST /api/v1/index/pause
POST /api/v1/index/resume
GET  /api/v1/config
PUT  /api/v1/config
```

### 搜尋 Response

```json
{
  "total_hits": 42,
  "duration_ms": 18,
  "results": [
    {
      "doc_id": "a1b2c3d4",
      "path": "C:\\docs\\report.pdf",
      "filename": "report.pdf",
      "title": "CUDA V100 Performance Report",
      "snippet": "...the CUDA architecture on <em>V100</em> provides...",
      "score": 0.95,
      "match_reason": "combined",
      "modified_at": 1709251200,
      "size_bytes": 2412544,
      "mime_type": "application/pdf",
      "source_type": "file"
    }
  ]
}
```

---

*本文件為活文件，每 Sprint 末應更新一次。如有架構決策變更，請在對應章節加入 ADR（Architecture Decision Record）標記。*
