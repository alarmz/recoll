# Recoll Next — 技術設計書 (TDD)
**版本**: v0.1-draft  
**狀態**: 供工程團隊審核  
**核心語言**: Rust edition 2021  
**目標平台**: Windows 10/11/Server 2019+ x64（Linux 為次要支援）

---

## 目錄

1. [Workspace 結構與模組邊界](#1-workspace-結構與模組邊界)
2. [核心資料模型 (rn-core)](#2-核心資料模型-rn-core)
3. [Rust Indexer + Pipeline (rn-indexer)](#3-rust-indexer--pipeline-rn-indexer)
4. [Tantivy Schema + 查詢設計 (rn-search)](#4-tantivy-schema--查詢設計-rn-search)
5. [Metadata Index (rn-meta)](#5-metadata-index-rn-meta)
6. [文件內容抽取器 (rn-extractors)](#6-文件內容抽取器-rn-extractors)
7. [Windows Service + 安裝包 (rn-windows)](#7-windows-service--安裝包-rn-windows)
8. [GPU 加速架構 (rn-gpu)](#8-gpu-加速架構-rn-gpu)
9. [設定系統](#9-設定系統)
10. [錯誤處理策略](#10-錯誤處理策略)
11. [測試策略](#11-測試策略)
12. [Phase 0 PoC 工作清單](#12-phase-0-poc-工作清單)

---

## 1. Workspace 結構與模組邊界

### 1.1 Cargo Workspace 佈局

```
recoll-next/
├── Cargo.toml                    # [workspace] members = [...]
├── Cargo.lock
├── crates/
│   ├── rn-core/                  # domain models, errors, config, events (lib)
│   ├── rn-indexer/               # main indexer service (bin + lib)
│   ├── rn-extractors/            # extractor trait + format impls (lib)
│   ├── rn-search/                # Tantivy schema, query parsing, ranking (lib)
│   ├── rn-meta/                  # SQLite metadata index (lib)
│   ├── rn-gpu/                   # GPU dispatch + CUDA backend (lib, feature-gated)
│   ├── rn-windows/               # Windows service, FSWatcher, installer helpers (lib)
│   ├── rn-gui/                   # Tauri 2.x desktop app (bin)
│   ├── rn-cli/                   # CLI binary (bin)
│   └── rn-sdk/                   # Public API: Rust lib + C FFI + HTTP server (lib+bin)
├── tools/
│   ├── bench/                    # Criterion benchmarks
│   └── smoke/                    # End-to-end smoke test harness
└── installer/
    ├── wix/                      # WiX 4 .wxs source
    └── build.rs                  # installer build script
```

### 1.2 Crate 相依規則（強制單向）

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

### 1.3 主要外部相依版本鎖定

| Crate | 版本 | 用途 |
|-------|------|------|
| `tantivy` | 0.22 | 全文搜尋核心 |
| `tokio` | 1.x (full features) | 非同步執行時 |
| `rusqlite` | 0.31 + bundled | Metadata SQLite |
| `serde` / `serde_json` | 1.x | 序列化 |
| `tracing` / `tracing-subscriber` | 0.1.x | 結構化日誌 |
| `anyhow` | 1.x | 應用層錯誤 |
| `thiserror` | 1.x | 函式庫層錯誤 |
| `uuid` | 1.x (v4, serde) | Task ID |
| `crossbeam-channel` | 0.5 | Worker 溝通 |
| `rayon` | 1.x | CPU-bound 並行 |
| `notify` | 6.x | 跨平台 FSWatcher |
| `windows-service` | 0.6 | Windows SCM |
| `tauri` | 2.x | GUI |
| `clap` | 4.x (derive) | CLI |
| `jieba-rs` | 0.7 | CJK 分詞 |
| `pdf-extract` | 0.7 | PDF text layer |
| `docx-rs` | 0.4 | DOCX 抽取 |

---

## 2. 核心資料模型 (rn-core)

### 2.1 IndexTask

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
    Reconcile,   // 定期校正掃描
}
```

### 2.2 DocumentState（狀態機）

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
| Queued | Extracting | Extract worker 取得 task |
| Extracting | Extracted | extract() 成功 |
| Extracted | Normalized | normalize worker 完成 |
| Normalized | Indexed | Tantivy writer commit 成功 |
| 任何 | Failed | 任何階段拋出 error |
| Failed | Queued | retry_count < MAX_RETRY (預設 3) |
| Indexed | Stale | Watcher 偵測到 mtime/size 變更 |
| Stale | Queued | 重新排入 Update task |
| Indexed/Stale | Deleted | Watcher 偵測到刪除事件 |
| Deleted | (tombstone) | Tombstone worker 清除 Tantivy + metadata |

### 2.3 ExtractResult

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

### 2.4 SearchResult

```rust
// crates/rn-core/src/search.rs

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchResult {
    pub doc_id:        String,    // Tantivy doc_id
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
```

---

## 3. Rust Indexer + Pipeline (rn-indexer)

### 3.1 整體 Pipeline 架構

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

### 3.2 IndexerService 主結構

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

### 3.3 TaskQueue

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

### 3.4 Worker Pool 設計

每種 worker 是獨立的 Tokio task，透過共享 Arc 存取資源。

#### 3.4.1 ExtractWorker

```rust
// crates/rn-indexer/src/workers/extract.rs

pub struct ExtractWorker {
    id:           usize,
    queue:        Arc<TaskQueue>,
    extractors:   Arc<ExtractorRegistry>,   // rn-extractors
    meta_store:   Arc<MetaStore>,
    output_tx:    Sender<ExtractedDoc>,      // 輸出到 NormalizeWorker
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
            // 指數退避重新排入
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

#### 3.4.2 NormalizeWorker

```rust
// crates/rn-indexer/src/workers/normalize.rs

pub struct NormalizeWorker {
    id:        usize,
    input_rx:  Receiver<ExtractedDoc>,
    output_tx: Sender<NormalizedDoc>,
    tokenizer: Arc<TokenizerPipeline>,   // rn-search
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

#### 3.4.3 IndexWriter（單一 writer，串行 commit）

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
                            self.meta_store.set_state(&doc.task.file_path, DocumentState::Indexed).await.ok();
                            self.pending += 1;
                            if self.should_commit() {
                                self.do_commit().await.ok();
                            }
                        }
                        Err(_) => break, // channel closed
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

#### 3.4.4 TombstoneWorker

```rust
// crates/rn-indexer/src/workers/tombstone.rs

pub struct TombstoneWorker {
    input_rx:    Receiver<DeleteTask>,
    writer:      Arc<Mutex<tantivy::IndexWriter>>,  // 與 IndexWriterWorker 共享
    meta_store:  Arc<MetaStore>,
    shutdown:    CancellationToken,
}

pub struct DeleteTask {
    pub file_path: PathBuf,
    pub doc_id:    Option<String>,  // 若已知則直接刪除，否則查 meta
}

impl TombstoneWorker {
    async fn delete(&self, task: DeleteTask) -> anyhow::Result<()> {
        let doc_id = match task.doc_id {
            Some(id) => id,
            None => self.meta_store.get_doc_id(&task.file_path).await?
                        .ok_or_else(|| anyhow::anyhow!("doc_id not found"))?,
        };

        // 刪除 Tantivy 文件
        let term = tantivy::Term::from_field_text(DOC_ID_FIELD, &doc_id);
        self.writer.lock().unwrap().delete_term(term);

        // 刪除 metadata
        self.meta_store.delete_by_path(&task.file_path).await?;

        tracing::info!(path = ?task.file_path, "tombstone ok");
        Ok(())
    }
}
```

### 3.5 初始掃描 Crawler

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

                // 比對 metadata store，若 mtime + size 相同則跳過
                let path = entry.path().to_owned();
                if self.meta_store.is_up_to_date(&path, &metadata).await? {
                    stats.skipped += 1;
                    continue;
                }

                self.queue.push(IndexTask {
                    task_id:      Uuid::new_v4(),
                    file_path:    path,
                    file_version: FileVersion::from_metadata(&metadata),
                    priority:     TaskPriority::Low,  // 初次掃描低優先
                    operation:    OperationType::Create,
                    source:       TaskSource::InitialScan,
                    scheduled_at: SystemTime::now(),
                    retry_count:  0,
                });
                stats.queued += 1;
            }
            // IO throttle: 每 queued % 1000 == 0 時 sleep
            if stats.queued % 1000 == 0 {
                self.throttle.yield_if_needed().await;
            }
        }
        Ok(stats)
    }

    fn is_excluded(&self, entry: &walkdir::DirEntry) -> bool {
        self.config.exclude_patterns.iter().any(|p|
            p.matches_path(entry.path())
        )
    }
}
```

### 3.6 IO Throttle

```rust
// crates/rn-indexer/src/throttle.rs

pub struct IoThrottle {
    mode: ThrottleMode,
}

pub enum ThrottleMode {
    /// 不限速
    Off,
    /// 每處理 N 個檔案，sleep M ms
    Gentle { files_per_batch: u32, sleep_ms: u64 },
    /// 限制 CPU 使用率上限（粗略實作：監測 wall clock）
    CpuCap { max_percent: u8 },
}

impl IoThrottle {
    pub async fn yield_if_needed(&self) {
        match self.mode {
            ThrottleMode::Off => {}
            ThrottleMode::Gentle { sleep_ms, .. } => {
                tokio::time::sleep(Duration::from_millis(sleep_ms)).await;
            }
            ThrottleMode::CpuCap { max_percent } => {
                // 實作：量測自上次 yield 的 elapsed，若過快則 sleep
                // 粗略版：sleep proportional to usage
            }
        }
    }
}
```

---

## 4. Tantivy Schema + 查詢設計 (rn-search)

### 4.1 Schema 定義

```rust
// crates/rn-search/src/schema.rs

use tantivy::schema::*;

pub struct RnSchema {
    pub schema:       Schema,
    // stored + indexed fields
    pub doc_id:       Field,   // STRING | STORED
    pub path:         Field,   // TEXT(raw) + STRING + STORED
    pub filename:     Field,   // TEXT + STRING + STORED
    pub title:        Field,   // TEXT + STORED
    pub extension:    Field,   // STRING + STORED | FAST
    pub mime_type:    Field,   // STRING + STORED
    pub content:      Field,   // TEXT (NOT stored — 節省磁碟)
    pub summary:      Field,   // STORED only (前 512 字元，供 snippet)
    pub language:     Field,   // STRING + STORED
    pub created_at:   Field,   // I64 | FAST + STORED
    pub modified_at:  Field,   // I64 | FAST + STORED
    pub indexed_at:   Field,   // I64 | FAST + STORED
    pub size_bytes:   Field,   // U64 | FAST + STORED
    pub source_type:  Field,   // STRING + STORED
    pub tags:         Field,   // TEXT + STORED
    pub checksum:     Field,   // STRING + STORED
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

        let string_stored = STRING | STORED;
        let i64_fast = NumericOptions::default().set_fast().set_stored();
        let u64_fast = NumericOptions::default().set_fast().set_stored();

        Self {
            doc_id:      builder.add_text_field("doc_id",      STRING | STORED),
            path:        builder.add_text_field("path",        string_stored),
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

### 4.2 Tokenizer Pipeline

```rust
// crates/rn-search/src/tokenizer.rs

use tantivy::tokenizer::*;

/// 向 Tantivy TokenizerManager 註冊所有自訂 tokenizer
pub fn register_tokenizers(index: &tantivy::Index) {
    let manager = index.tokenizers();

    // 英文 / 通用文字：小寫 + unicode 邊界切詞 + stopword 過濾
    manager.register("rn_default",
        TextAnalyzer::builder(SimpleTokenizer::default())
            .filter(LowerCaser)
            .filter(StopWordFilter::remove(DEFAULT_STOPWORDS.iter().copied()))
            .filter(Stemmer::new(Language::English))
            .build(),
    );

    // CJK：先嘗試 jieba，fallback bigram
    manager.register("rn_cjk",
        TextAnalyzer::builder(JiebaTokenizer::new())  // 自製 wrapper
            .filter(LowerCaser)
            .build(),
    );

    // Source code：保留 _ 與 . 作為詞邊界內字元
    manager.register("rn_code",
        TextAnalyzer::builder(CodeTokenizer::new())   // 自製
            .filter(LowerCaser)
            .build(),
    );

    // Filename：額外分割 - _ . 但保留完整字串
    manager.register("rn_filename",
        TextAnalyzer::builder(FilenameTokenizer::new())
            .filter(LowerCaser)
            .build(),
    );
}
```

#### 4.2.1 JiebaTokenizer（自製 Tantivy wrapper）

```rust
// crates/rn-search/src/tokenizer/jieba.rs

use jieba_rs::Jieba;
use tantivy::tokenizer::{BoxTokenStream, Token, TokenStream, Tokenizer};
use std::sync::Arc;

#[derive(Clone)]
pub struct JiebaTokenizer {
    jieba: Arc<Jieba>,
}

impl JiebaTokenizer {
    pub fn new() -> Self {
        Self { jieba: Arc::new(Jieba::new()) }
    }
}

impl Tokenizer for JiebaTokenizer {
    type TokenStream<'a> = JiebaTokenStream;

    fn token_stream<'a>(&'a mut self, text: &'a str) -> Self::TokenStream<'a> {
        let tokens: Vec<Token> = self.jieba.cut(text, true)
            .iter()
            .scan(0usize, |offset, word| {
                let start = text[*offset..].find(word).map(|i| *offset + i).unwrap_or(*offset);
                let end = start + word.len();
                *offset = end;
                Some(Token {
                    offset_from: start,
                    offset_to:   end,
                    position:    0,      // filled by pipeline
                    text:        word.to_string(),
                    position_length: 1,
                })
            })
            .collect();
        JiebaTokenStream { tokens, index: 0 }
    }
}
```

### 4.3 Query Parser

```rust
// crates/rn-search/src/query.rs

use tantivy::query::*;
use tantivy::schema::Field;

pub struct QueryParser {
    schema:          RnSchema,
    default_fields:  Vec<Field>,    // [filename, title, content]
    tantivy_parser:  tantivy::query::QueryParser,
}

/// 使用者可輸入的查詢格式
pub enum ParsedQuery {
    /// 關鍵字搜尋（多 field）
    Keyword(Box<dyn Query>),
    /// 帶 field: 前綴的欄位查詢
    Field { field: String, query: Box<dyn Query> },
    /// 路徑過濾
    PathScoped { path_prefix: PathBuf, inner: Box<ParsedQuery> },
    /// 時間過濾
    DateScoped { from: Option<i64>, to: Option<i64>, inner: Box<ParsedQuery> },
    /// 複合查詢
    Boolean { clauses: Vec<(Occur, Box<ParsedQuery>)> },
}

impl QueryParser {
    pub fn new(schema: RnSchema) -> Self {
        let default_fields = vec![
            schema.filename,
            schema.title,
            schema.content,
        ];
        let parser = tantivy::query::QueryParser::for_index(
            /* index */ ...,
            default_fields.clone(),
        );
        Self { schema, default_fields, tantivy_parser: parser }
    }

    /// 解析使用者輸入，支援以下語法：
    ///   cuda v100
    ///   "cuda toolkit"
    ///   filename:report.pdf
    ///   ext:docx
    ///   modified:>2024-01-01
    ///   path:D:\projects\*
    pub fn parse(&self, input: &str) -> anyhow::Result<Box<dyn Query>> {
        let input = input.trim();

        // 1. 路徑前綴偵測
        if let Some(path) = extract_path_filter(input) {
            let inner = self.parse_inner(strip_path(input))?;
            return Ok(Box::new(PathScopedQuery::new(path, inner)));
        }

        // 2. 日期範圍偵測
        if let Some((range, rest)) = extract_date_filter(input) {
            let inner = self.parse_inner(rest)?;
            return Ok(Box::new(RangeQuery::new_term_bounds(
                self.schema.modified_at, range,
            )));
        }

        self.parse_inner(input)
    }

    fn parse_inner(&self, input: &str) -> anyhow::Result<Box<dyn Query>> {
        // 偵測 field:value 語法
        if let Some((field_name, value)) = input.split_once(':') {
            if let Ok(field) = self.schema.schema.get_field(field_name) {
                return Ok(Box::new(TermQuery::new(
                    tantivy::Term::from_field_text(field, value),
                    IndexRecordOption::Basic,
                )));
            }
        }
        // fallback: Tantivy 標準 QueryParser
        Ok(self.tantivy_parser.parse_query(input)?)
    }
}
```

### 4.4 Ranking / Scoring

```rust
// crates/rn-search/src/ranking.rs

/// 最終分數 = a * fulltext_bm25 + b * filename_score + c * recency_score
/// 第一期 semantic_score = 0.0
pub struct RankingWeights {
    pub fulltext:  f32,   // 預設 0.60
    pub filename:  f32,   // 預設 0.30
    pub recency:   f32,   // 預設 0.10
    pub semantic:  f32,   // 預設 0.00（第二期啟用）
}

impl Default for RankingWeights {
    fn default() -> Self {
        Self { fulltext: 0.60, filename: 0.30, recency: 0.10, semantic: 0.00 }
    }
}

pub fn compute_filename_score(query_tokens: &[&str], filename: &str) -> f32 {
    let fname_lower = filename.to_lowercase();
    let exact_match = query_tokens.iter().all(|t| fname_lower.contains(*t));
    let prefix_match = query_tokens.iter().any(|t| fname_lower.starts_with(*t));

    if exact_match  { 1.0 }
    else if prefix_match { 0.7 }
    else { query_tokens.iter()
        .filter(|t| fname_lower.contains(**t))
        .count() as f32 / query_tokens.len() as f32 * 0.5
    }
}

pub fn compute_recency_score(modified_at: i64) -> f32 {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH).unwrap().as_secs() as i64;
    let age_days = (now - modified_at) / 86400;
    // 衰減曲線：30 天內 = 1.0，365 天 = 0.5，> 3 年 = 0.1
    (1.0_f32 / (1.0 + age_days as f32 / 30.0)).max(0.1)
}
```

### 4.5 Snippet 生成

Tantivy 內建 `SnippetGenerator`，但需針對 CJK 做額外處理。

```rust
// crates/rn-search/src/snippet.rs

use tantivy::snippet::SnippetGenerator;

pub struct SnippetBuilder {
    max_fragment_chars: usize,   // 預設 200
    highlight_open:     String,  // 預設 "<em>"
    highlight_close:    String,  // 預設 "</em>"
}

impl SnippetBuilder {
    pub fn generate(
        &self,
        searcher: &tantivy::Searcher,
        query:    &dyn tantivy::query::Query,
        doc:      &tantivy::Document,
        field:    Field,
    ) -> String {
        // 優先使用 content field；若無則 fallback 到 summary
        let gen = SnippetGenerator::create(searcher, query, field)
            .expect("snippet generator");
        let snippet = gen.snippet_from_doc(doc);
        snippet.to_html()
    }
}
```

---

## 5. Metadata Index (rn-meta)

### 5.1 SQLite Schema

```sql
-- crates/rn-meta/migrations/001_init.sql
-- 開啟 WAL 模式，提升並發讀取效能
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

-- 用於 LIKE 前綴查詢加速（需要 LIKE 'prefix%' 形式）
CREATE INDEX IF NOT EXISTS idx_files_filename_lower ON files(lower(filename));
```

### 5.2 MetaStore 介面

```rust
// crates/rn-meta/src/store.rs

use rusqlite::{Connection, params};
use std::sync::{Arc, Mutex};

pub struct MetaStore {
    conn: Arc<Mutex<Connection>>,   // 單一連線，WAL 模式下可安全多 reader
}

impl MetaStore {
    pub fn open(db_path: &Path) -> anyhow::Result<Self> {
        let conn = Connection::open(db_path)?;
        conn.execute_batch(include_str!("../migrations/001_init.sql"))?;
        conn.execute_batch("PRAGMA cache_size = -32000;")?; // 32 MB cache
        Ok(Self { conn: Arc::new(Mutex::new(conn)) })
    }

    pub async fn is_up_to_date(&self, path: &Path, meta: &std::fs::Metadata) -> anyhow::Result<bool> {
        let conn = self.conn.lock().unwrap();
        let result: Option<(i64, i64)> = conn.query_row(
            "SELECT modified_at, size FROM files WHERE path = ?1 AND index_state = 'indexed'",
            params![path.to_string_lossy()],
            |row| Ok((row.get(0)?, row.get(1)?)),
        ).optional()?;

        Ok(result.map_or(false, |(mtime, size)| {
            let meta_mtime = meta.modified().ok()
                .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
                .map(|d| d.as_secs() as i64)
                .unwrap_or(0);
            mtime == meta_mtime && size == meta.len() as i64
        }))
    }

    pub async fn upsert_metadata(&self, record: &MetadataRecord) -> anyhow::Result<()> {
        let conn = self.conn.lock().unwrap();
        conn.execute(
            r#"INSERT INTO files
               (file_id, path, filename, extension, size, modified_at, mime_type, index_state, doc_id, indexed_at, updated_at)
               VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 'indexed', ?8, unixepoch(), unixepoch())
               ON CONFLICT(path) DO UPDATE SET
                   size        = excluded.size,
                   modified_at = excluded.modified_at,
                   mime_type   = excluded.mime_type,
                   index_state = 'indexed',
                   doc_id      = excluded.doc_id,
                   indexed_at  = excluded.indexed_at,
                   updated_at  = unixepoch()"#,
            params![
                record.file_id.to_string(),
                record.path.to_string_lossy(),
                record.filename,
                record.extension,
                record.size as i64,
                record.modified_at,
                record.mime_type,
                record.doc_id,
            ],
        )?;
        Ok(())
    }

    /// 快速檔名前綴搜尋，給 instant search 用
    pub async fn search_filename_prefix(&self, prefix: &str, limit: usize) -> anyhow::Result<Vec<FileMeta>> {
        let conn = self.conn.lock().unwrap();
        let pattern = format!("{}%", prefix.to_lowercase());
        let mut stmt = conn.prepare(
            "SELECT path, filename, extension, size, modified_at, mime_type
             FROM files
             WHERE lower(filename) LIKE ?1 AND index_state = 'indexed'
             ORDER BY modified_at DESC
             LIMIT ?2"
        )?;
        let rows = stmt.query_map(params![pattern, limit as i64], |row| {
            Ok(FileMeta {
                path:        PathBuf::from(row.get::<_, String>(0)?),
                filename:    row.get(1)?,
                extension:   row.get(2)?,
                size:        row.get::<_, i64>(3)? as u64,
                modified_at: row.get(4)?,
                mime_type:   row.get(5)?,
            })
        })?;
        rows.collect::<Result<Vec<_>, _>>().map_err(Into::into)
    }

    pub async fn set_state(&self, path: &Path, state: DocumentState) -> anyhow::Result<()> { ... }
    pub async fn get_doc_id(&self, path: &Path) -> anyhow::Result<Option<String>> { ... }
    pub async fn delete_by_path(&self, path: &Path) -> anyhow::Result<()> { ... }
    pub async fn get_failed_tasks(&self, limit: usize) -> anyhow::Result<Vec<IndexTask>> { ... }
}
```

---

## 6. 文件內容抽取器 (rn-extractors)

### 6.1 Extractor Trait

```rust
// crates/rn-extractors/src/lib.rs

use async_trait::async_trait;

#[async_trait]
pub trait Extractor: Send + Sync {
    /// 判斷此 extractor 是否支援該檔案
    fn supports(&self, mime: &str, extension: &str) -> bool;

    /// 抽取內容
    async fn extract(&self, path: &Path) -> anyhow::Result<ExtractResult>;

    /// 抽取 metadata（不讀全文，成本低）
    async fn extract_metadata(&self, path: &Path) -> anyhow::Result<FileMeta>;

    /// 估算抽取成本（用於優先排程 GPU offload）
    fn cost_estimate(&self, size: u64) -> CostProfile;

    /// extractor 名稱（用於 log）
    fn name(&self) -> &'static str;
}

#[derive(Debug, Clone)]
pub struct CostProfile {
    pub cpu_intensity:    CostLevel,  // Low / Medium / High
    pub memory_estimate:  u64,        // bytes
    pub gpu_offloadable:  bool,
}

#[derive(Debug, Clone, Copy)]
pub enum CostLevel { Low, Medium, High }
```

### 6.2 ExtractorRegistry

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
                Box::new(FallbackExtractor::new()),   // 最後防線：嘗試 UTF-8 讀取
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

        tracing::debug!(
            extractor = extractor.name(),
            path = ?task.file_path,
            "extracting"
        );
        extractor.extract(&task.file_path).await
    }
}
```

### 6.3 PdfExtractor 實作

```rust
// crates/rn-extractors/src/pdf.rs

pub struct PdfExtractor {
    max_pages: u32,       // 預設 500
    ocr_fallback: bool,   // 預設 true
}

#[async_trait]
impl Extractor for PdfExtractor {
    fn supports(&self, mime: &str, ext: &str) -> bool {
        mime == "application/pdf" || ext == "pdf"
    }

    async fn extract(&self, path: &Path) -> anyhow::Result<ExtractResult> {
        let start = Instant::now();

        // 嘗試 pdf-extract（文字層）
        match pdf_extract::extract_text(path) {
            Ok(text) if !text.trim().is_empty() => {
                return Ok(ExtractResult {
                    raw_text: text,
                    extraction_method: ExtractionMethod::Native,
                    extraction_time_ms: start.elapsed().as_millis() as u64,
                    ..Default::default()
                });
            }
            _ => {}
        }

        // Fallback: OCR（若啟用）
        if self.ocr_fallback {
            return self.extract_via_ocr(path, start).await;
        }

        Err(anyhow::anyhow!("PDF has no text layer and OCR is disabled"))
    }

    fn cost_estimate(&self, size: u64) -> CostProfile {
        CostProfile {
            cpu_intensity:   if size > 10_000_000 { CostLevel::High } else { CostLevel::Medium },
            memory_estimate: size * 3,  // 粗估：文字展開 3x
            gpu_offloadable: true,      // OCR path 可 GPU offload
        }
    }

    fn name(&self) -> &'static str { "pdf" }
}
```

---

## 7. Windows Service + 安裝包 (rn-windows)

### 7.1 Windows Service 實作

```rust
// crates/rn-windows/src/service.rs

use windows_service::{
    define_windows_service,
    service::{
        ServiceControl, ServiceControlAccept, ServiceExitCode,
        ServiceState, ServiceStatus, ServiceType,
    },
    service_control_handler::{self, ServiceControlHandlerResult},
    service_dispatcher,
};

const SERVICE_NAME: &str = "RecollNextIndexer";
const SERVICE_DISPLAY: &str = "Recoll Next Indexer Service";
const SERVICE_DESC: &str = "Indexes local files for fast full-text search";

define_windows_service!(ffi_service_main, service_main);

pub fn run_as_service() -> windows_service::Result<()> {
    service_dispatcher::start(SERVICE_NAME, ffi_service_main)
}

fn service_main(arguments: Vec<OsString>) {
    let rt = tokio::runtime::Runtime::new().unwrap();
    rt.block_on(async_service_main(arguments)).unwrap_or_else(|e| {
        tracing::error!("service error: {e}");
    });
}

async fn async_service_main(_args: Vec<OsString>) {
    let config = IndexerConfig::load_default().await.unwrap();
    let service = IndexerService::start(config).await.unwrap();

    let (shutdown_tx, shutdown_rx) = tokio::sync::oneshot::channel::<()>();
    let shutdown_tx = Arc::new(Mutex::new(Some(shutdown_tx)));

    let status_handle = service_control_handler::register(
        SERVICE_NAME,
        move |control| -> ServiceControlHandlerResult {
            match control {
                ServiceControl::Stop | ServiceControl::Shutdown => {
                    if let Some(tx) = shutdown_tx.lock().unwrap().take() {
                        let _ = tx.send(());
                    }
                    ServiceControlHandlerResult::NoError
                }
                ServiceControl::Interrogate => ServiceControlHandlerResult::NoError,
                _ => ServiceControlHandlerResult::NotImplemented,
            }
        },
    ).unwrap();

    // 回報 Running
    status_handle.set_service_status(ServiceStatus {
        service_type:             ServiceType::OWN_PROCESS,
        current_state:            ServiceState::Running,
        controls_accepted:        ServiceControlAccept::STOP | ServiceControlAccept::SHUTDOWN,
        exit_code:                ServiceExitCode::Win32(0),
        checkpoint:               0,
        wait_hint:                Duration::default(),
        process_id:               None,
    }).unwrap();

    // 等待 shutdown 訊號
    let _ = shutdown_rx.await;

    // 回報 Stopping
    status_handle.set_service_status(ServiceStatus {
        current_state: ServiceState::StopPending,
        ..Default::default()
    }).unwrap();

    service.shutdown().await.ok();

    status_handle.set_service_status(ServiceStatus {
        current_state: ServiceState::Stopped,
        exit_code:     ServiceExitCode::Win32(0),
        ..Default::default()
    }).unwrap();
}
```

### 7.2 Service Install / Uninstall CLI

```rust
// crates/rn-windows/src/install.rs
use windows_service::service_manager::{ServiceManager, ServiceManagerAccess};
use windows_service::service::{ServiceAccess, ServiceErrorControl, ServiceInfo, ServiceStartType, ServiceType};

pub fn install_service(binary_path: &Path) -> anyhow::Result<()> {
    let manager = ServiceManager::local_computer(
        None::<&str>,
        ServiceManagerAccess::CONNECT | ServiceManagerAccess::CREATE_SERVICE,
    )?;

    let info = ServiceInfo {
        name:             OsString::from(SERVICE_NAME),
        display_name:     OsString::from(SERVICE_DISPLAY),
        service_type:     ServiceType::OWN_PROCESS,
        start_type:       ServiceStartType::AutoStart,
        error_control:    ServiceErrorControl::Normal,
        executable_path:  binary_path.to_owned(),
        launch_arguments: vec![OsString::from("--service")],
        dependencies:     vec![],
        account_name:     None,   // LocalSystem
        account_password: None,
    };

    let _service = manager.create_service(&info, ServiceAccess::CHANGE_CONFIG)?;
    tracing::info!("service installed: {}", SERVICE_NAME);
    Ok(())
}

pub fn uninstall_service() -> anyhow::Result<()> {
    let manager = ServiceManager::local_computer(None::<&str>, ServiceManagerAccess::CONNECT)?;
    let service = manager.open_service(SERVICE_NAME, ServiceAccess::DELETE | ServiceAccess::STOP)?;
    service.stop()?;
    std::thread::sleep(Duration::from_secs(2));
    service.delete()?;
    tracing::info!("service uninstalled: {}", SERVICE_NAME);
    Ok(())
}
```

### 7.3 FSWatcher（Windows 原生 + notify）

```rust
// crates/rn-windows/src/watcher.rs

use notify::{Config, Event, RecommendedWatcher, RecursiveMode, Watcher};
use std::sync::mpsc;

pub struct FsWatcher {
    _watcher:    RecommendedWatcher,  // 持有以保持 alive
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

pub struct Debouncer {
    window_ms: u64,
    pending:   Mutex<HashMap<PathBuf, Instant>>,
}

impl Debouncer {
    /// 若同一路徑在 window_ms 內已有 pending event，更新時間戳並回傳 false（壓制）
    pub fn should_emit(&self, path: &PathBuf) -> bool {
        let mut pending = self.pending.lock().unwrap();
        let now = Instant::now();
        if let Some(prev) = pending.get(path) {
            if now.duration_since(*prev).as_millis() < self.window_ms as u128 {
                pending.insert(path.clone(), now);
                return false;
            }
        }
        pending.insert(path.clone(), now);
        true
    }
}

impl FsWatcher {
    pub fn new(paths: &[PathBuf], event_tx: Sender<WatchEvent>) -> anyhow::Result<Self> {
        let debouncer = Arc::new(Debouncer { window_ms: 500, pending: Mutex::new(HashMap::new()) });
        let deb_clone = debouncer.clone();
        let tx_clone  = event_tx.clone();

        let mut watcher = RecommendedWatcher::new(
            move |res: notify::Result<Event>| {
                if let Ok(event) = res {
                    for path in event.paths {
                        let kind = match event.kind {
                            notify::EventKind::Create(_) => WatchEventKind::Created,
                            notify::EventKind::Modify(_) => WatchEventKind::Modified,
                            notify::EventKind::Remove(_) => WatchEventKind::Deleted,
                            _ => return,
                        };
                        if deb_clone.should_emit(&path) {
                            let _ = tx_clone.send(WatchEvent { kind, path, ts: Instant::now() });
                        }
                    }
                }
            },
            Config::default(),
        )?;

        for path in paths {
            watcher.watch(path, RecursiveMode::Recursive)?;
        }

        Ok(Self { _watcher: watcher, event_tx, debouncer })
    }
}
```

### 7.4 WiX 4 安裝包結構

```xml
<!-- installer/wix/main.wxs -->
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Package Name="Recoll Next"
           Manufacturer="YourCompany"
           Version="!(bind.fileVersion.FileRecollNextIndexer)"
           UpgradeCode="{YOUR-UPGRADE-GUID}"
           Scope="perMachine">

    <MajorUpgrade DowngradeErrorMessage="newer version installed" />
    <MediaTemplate EmbedCab="yes" />

    <Feature Id="ProductFeature" Title="Recoll Next" Level="1">
      <ComponentGroupRef Id="ProductComponents" />
      <ComponentGroupRef Id="ServiceComponent" />
    </Feature>
  </Package>

  <Fragment>
    <Directory Id="TARGETDIR" Name="SourceDir">
      <Directory Id="ProgramFiles64Folder">
        <Directory Id="INSTALLFOLDER" Name="RecollNext" />
      </Directory>
      <Directory Id="ProgramMenuFolder">
        <Directory Id="ApplicationProgramsFolder" Name="Recoll Next" />
      </Directory>
    </Directory>
  </Fragment>

  <Fragment>
    <ComponentGroup Id="ProductComponents" Directory="INSTALLFOLDER">
      <!-- indexer service binary -->
      <Component Id="CmpIndexer" Guid="{INDEXER-GUID}">
        <File Id="FileRecollNextIndexer" Source="$(var.BinDir)\rn-indexer.exe"
              KeyPath="yes" />
      </Component>
      <!-- CLI -->
      <Component Id="CmpCli" Guid="{CLI-GUID}">
        <File Source="$(var.BinDir)\rn-cli.exe" />
      </Component>
      <!-- GUI -->
      <Component Id="CmpGui" Guid="{GUI-GUID}">
        <File Source="$(var.BinDir)\rn-gui.exe" />
        <Shortcut Id="ShortcutGui" Directory="ApplicationProgramsFolder"
                  Name="Recoll Next" WorkingDirectory="INSTALLFOLDER"
                  Advertise="no" />
      </Component>
    </ComponentGroup>

    <!-- Windows Service 安裝 -->
    <ComponentGroup Id="ServiceComponent" Directory="INSTALLFOLDER">
      <Component Id="CmpService" Guid="{SERVICE-GUID}">
        <ServiceInstall Id="SvcInstall"
                        Name="RecollNextIndexer"
                        DisplayName="Recoll Next Indexer"
                        Description="Indexes local files for fast search"
                        Type="ownProcess"
                        Start="auto"
                        ErrorControl="normal"
                        Arguments="--service" />
        <ServiceControl Id="SvcStart"  Name="RecollNextIndexer"
                        Start="install" Stop="both" Remove="uninstall" Wait="yes" />
      </Component>
    </ComponentGroup>
  </Fragment>
</Wix>
```

### 7.5 CI/CD Windows Build Pipeline（GitHub Actions）

```yaml
# .github/workflows/windows-release.yml
name: Windows Release Build

on:
  push:
    tags: ['v*']

jobs:
  build:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4

      - name: Install Rust MSVC toolchain
        uses: dtolnay/rust-toolchain@stable
        with:
          targets: x86_64-pc-windows-msvc

      - name: Rust cache
        uses: Swatinem/rust-cache@v2

      - name: Build release
        run: cargo build --release --target x86_64-pc-windows-msvc

      - name: Run tests
        run: cargo test --release --target x86_64-pc-windows-msvc

      - name: Clippy
        run: cargo clippy --target x86_64-pc-windows-msvc -- -D warnings

      - name: Install WiX 4
        run: dotnet tool install --global wix

      - name: Build MSI
        run: |
          wix build installer/wix/main.wxs `
            -d BinDir=target/x86_64-pc-windows-msvc/release `
            -o recoll-next-${{ github.ref_name }}-x64.msi

      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: recoll-next-windows
          path: |
            recoll-next-*.msi
            target/x86_64-pc-windows-msvc/release/rn-cli.exe
```

---

## 8. GPU 加速架構 (rn-gpu)

### 8.1 設計原則

GPU 加速採用**可選插拔**設計：

- `rn-gpu` 透過 Cargo feature `gpu` 啟用，disable 時整個 crate 不參與編譯
- `IndexerService` 中 `gpu_dispatcher: Option<Arc<GpuDispatcher>>`，`None` = CPU-only 模式
- GPU 任務採 **batch** 模式，每 batch 至少 `min_batch_size`（預設 32）個文件
- 單個小文件（< 10KB）不 dispatch 到 GPU（transfer overhead 可能大於收益）

### 8.2 GpuDispatcher 介面

```rust
// crates/rn-gpu/src/lib.rs

#[async_trait]
pub trait GpuBackend: Send + Sync {
    /// 批次文字前處理（Unicode cleaning + sentence split + token pre-segmentation）
    async fn batch_preprocess(
        &self,
        texts: Vec<String>,
    ) -> anyhow::Result<Vec<PreprocessResult>>;

    /// 批次 embedding（第二期）
    async fn batch_embed(
        &self,
        texts: Vec<String>,
    ) -> anyhow::Result<Vec<Embedding>>;

    fn device_info(&self) -> GpuDeviceInfo;
    fn is_available(&self) -> bool;
}

#[derive(Debug, Clone)]
pub struct PreprocessResult {
    pub cleaned_text: String,
    pub sentences:    Vec<String>,
    pub token_count:  usize,
}

#[derive(Debug, Clone)]
pub struct GpuDeviceInfo {
    pub vendor:      String,   // "NVIDIA" | "AMD" | "Intel"
    pub name:        String,
    pub vram_bytes:  u64,
    pub driver_ver:  String,
    pub backend:     GpuBackendType,
}

#[derive(Debug, Clone, Copy)]
pub enum GpuBackendType { Cuda, Rocm, DirectMl, Vulkan, None }
```

### 8.3 GpuDispatcher

```rust
// crates/rn-gpu/src/dispatcher.rs

pub struct GpuDispatcher {
    backend:        Arc<dyn GpuBackend>,
    mode:           GpuMode,
    min_batch_size: usize,         // 預設 32
    min_text_bytes: usize,         // 預設 10_240（< 10KB 不 dispatch）
    pending:        Mutex<Vec<DispatchJob>>,
    metrics:        Arc<GpuMetrics>,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum GpuMode {
    Off,
    PreprocessOnly,
    EmbeddingOnly,      // 第二期
    FullAccelerate,
    Auto,               // 依 batch size 自動決定
}

struct DispatchJob {
    task_id: Uuid,
    text:    String,
    tx:      tokio::sync::oneshot::Sender<PreprocessResult>,
}

impl GpuDispatcher {
    pub async fn submit_preprocess(&self, task_id: Uuid, text: String)
        -> anyhow::Result<PreprocessResult>
    {
        // 小文件直接 CPU 處理
        if text.len() < self.min_text_bytes {
            return Ok(cpu_preprocess(text));
        }

        let (tx, rx) = tokio::sync::oneshot::channel();
        self.pending.lock().unwrap().push(DispatchJob { task_id, text, tx });

        // 若積累到 batch 上限，立即 flush
        if self.pending.lock().unwrap().len() >= self.min_batch_size {
            self.flush().await?;
        }

        rx.await.map_err(Into::into)
    }

    async fn flush(&self) -> anyhow::Result<()> {
        let jobs: Vec<DispatchJob> = {
            let mut pending = self.pending.lock().unwrap();
            std::mem::take(&mut *pending)
        };
        if jobs.is_empty() { return Ok(()); }

        let texts: Vec<String> = jobs.iter().map(|j| j.text.clone()).collect();
        let results = self.backend.batch_preprocess(texts).await?;

        for (job, result) in jobs.into_iter().zip(results) {
            let _ = job.tx.send(result);
        }
        Ok(())
    }
}
```

### 8.4 CUDA Backend（第一期 PoC）

```rust
// crates/rn-gpu/src/backends/cuda.rs
// 第一期使用 cuBLAS / cuDNN via cudarc crate

#[cfg(feature = "cuda")]
pub struct CudaBackend {
    device: cudarc::driver::CudaDevice,
}

#[cfg(feature = "cuda")]
#[async_trait]
impl GpuBackend for CudaBackend {
    async fn batch_preprocess(&self, texts: Vec<String>) -> anyhow::Result<Vec<PreprocessResult>> {
        // Phase 0 PoC：先在 CPU 實作，量測 baseline
        // Phase 1：將 unicode cleaning kernel 移至 GPU
        tokio::task::spawn_blocking(move || {
            texts.into_iter().map(|t| cpu_preprocess(t)).collect::<Vec<_>>()
        }).await.map_err(Into::into)
    }

    fn device_info(&self) -> GpuDeviceInfo {
        GpuDeviceInfo {
            vendor:     "NVIDIA".into(),
            name:       self.device.name().unwrap_or_default(),
            vram_bytes: self.device.total_memory().unwrap_or(0),
            driver_ver: "unknown".into(),
            backend:    GpuBackendType::Cuda,
        }
    }

    fn is_available(&self) -> bool { true }
}
```

### 8.5 GPU Backend 選擇策略

```rust
// crates/rn-gpu/src/factory.rs

pub fn create_best_available_backend() -> Arc<dyn GpuBackend> {
    // 1. 嘗試 CUDA
    #[cfg(feature = "cuda")]
    if let Ok(backend) = CudaBackend::try_init() {
        tracing::info!("GPU backend: CUDA ({})", backend.device_info().name);
        return Arc::new(backend);
    }

    // 2. 嘗試 ROCm（AMD）
    #[cfg(feature = "rocm")]
    if let Ok(backend) = RocmBackend::try_init() {
        tracing::info!("GPU backend: ROCm");
        return Arc::new(backend);
    }

    // 3. Fallback: CPU-only（NullBackend）
    tracing::warn!("no GPU backend available, using CPU fallback");
    Arc::new(NullBackend)
}

/// NullBackend = 完整 CPU fallback，行為與 GpuBackend 介面一致
pub struct NullBackend;

#[async_trait]
impl GpuBackend for NullBackend {
    async fn batch_preprocess(&self, texts: Vec<String>) -> anyhow::Result<Vec<PreprocessResult>> {
        Ok(texts.into_iter().map(cpu_preprocess).collect())
    }
    fn device_info(&self) -> GpuDeviceInfo { GpuDeviceInfo { backend: GpuBackendType::None, ..Default::default() } }
    fn is_available(&self) -> bool { false }
    async fn batch_embed(&self, _: Vec<String>) -> anyhow::Result<Vec<Embedding>> {
        Err(anyhow::anyhow!("embedding not available in NullBackend"))
    }
}
```

---

## 9. 設定系統

### 9.1 設定檔格式（TOML）

```toml
# %APPDATA%\RecollNext\config.toml

[indexer]
root_paths        = ["C:\\Users\\username\\Documents", "D:\\Projects"]
exclude_patterns  = ["**\\.git\\**", "**\\node_modules\\**", "**\\target\\**"]
max_file_size_mb  = 100
max_content_mb    = 10
follow_symlinks   = false

[workers]
extract_workers    = 4
normalize_workers  = 4
max_cpu_percent    = 60       # 粗略限速
io_gentle_mode     = true
pause_on_battery   = true

[commit]
policy     = "hybrid"         # "by_count" | "by_time" | "hybrid"
count      = 500
time_secs  = 30

[gpu]
mode       = "auto"           # "off" | "preprocess_only" | "full_accelerate" | "auto"
min_batch  = 32
min_text_kb = 10

[search]
snippet_chars       = 200
max_results         = 50
ranking_fulltext    = 0.60
ranking_filename    = 0.30
ranking_recency     = 0.10

[logging]
level      = "info"           # error | warn | info | debug | trace
dir        = "%APPDATA%\\RecollNext\\logs"
max_size_mb = 50
max_backups = 5

[windows]
register_context_menu = true
start_with_windows    = true
show_tray_icon        = true
```

### 9.2 Config Struct

```rust
// crates/rn-core/src/config.rs

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppConfig {
    pub indexer:  IndexerConfig,
    pub workers:  WorkerConfig,
    pub commit:   CommitConfig,
    pub gpu:      GpuConfig,
    pub search:   SearchConfig,
    pub logging:  LogConfig,
    pub windows:  WindowsConfig,
}

impl AppConfig {
    /// 載入優先序: CLI args > user config > system config > defaults
    pub fn load(cli_overrides: &CliArgs) -> anyhow::Result<Self> {
        let user_path   = dirs::config_dir()
            .map(|d| d.join("RecollNext").join("config.toml"));
        let system_path = PathBuf::from(r"C:\ProgramData\RecollNext\config.toml");

        let mut config = Self::default();

        if system_path.exists() {
            config.merge(toml::from_str(&fs::read_to_string(&system_path)?)?);
        }
        if let Some(path) = user_path.filter(|p| p.exists()) {
            config.merge(toml::from_str(&fs::read_to_string(&path)?)?);
        }
        config.apply_cli(cli_overrides);
        Ok(config)
    }
}
```

---

## 10. 錯誤處理策略

### 10.1 錯誤分層

| 層次 | 型別 | 用途 |
|------|------|------|
| 函式庫 crate | `thiserror` enum | 明確語義，供呼叫者 match |
| 應用層 / workers | `anyhow::Error` | 快速包裝，保留 context |
| FFI / C ABI | `i32` error code | 不可傳遞 Rust 型別 |
| HTTP API | JSON `{ "error": "...", "code": 400 }` | 前端消費 |

### 10.2 核心 Error 型別

```rust
// crates/rn-core/src/error.rs

use thiserror::Error;

#[derive(Debug, Error)]
pub enum IndexError {
    #[error("extractor not found for {mime} / {ext}")]
    NoExtractor { mime: String, ext: String },

    #[error("extraction failed: {reason}")]
    ExtractionFailed { reason: String },

    #[error("tantivy error: {0}")]
    Tantivy(#[from] tantivy::TantivyError),

    #[error("metadata db error: {0}")]
    Database(#[from] rusqlite::Error),

    #[error("io error at {path}: {source}")]
    Io { path: String, #[source] source: std::io::Error },

    #[error("task max retries exceeded: {task_id}")]
    MaxRetriesExceeded { task_id: uuid::Uuid },

    #[error("gpu dispatch error: {0}")]
    Gpu(String),
}
```

### 10.3 Panic Recovery

```rust
// crates/rn-indexer/src/service.rs — worker 啟動

tokio::spawn(async move {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        tokio::runtime::Handle::current().block_on(worker.run())
    }));
    if let Err(panic) = result {
        tracing::error!("worker panicked, restarting in 5s");
        tokio::time::sleep(Duration::from_secs(5)).await;
        // 重新啟動 worker
    }
});
```

---

## 11. 測試策略

### 11.1 單元測試範圍

```
rn-core:        TaskPriority 排序、DocumentState 轉換合法性
rn-search:      QueryParser 各語法、scoring formula、snippet 邊界
rn-meta:        SQLite upsert、prefix search、is_up_to_date 邏輯
rn-extractors:  各 extractor supports() 判斷、fixture 檔案抽取結果
rn-gpu:         NullBackend fallback、batch flush 邏輯
rn-windows:     Debouncer suppress 邏輯、event kind mapping
```

### 11.2 整合測試（tools/smoke/）

```rust
// tools/smoke/src/basic_indexing.rs

#[tokio::test]
async fn smoke_initial_crawl() {
    // 1. 建立臨時目錄，放入已知 fixture 文件
    let dir = tempfile::tempdir().unwrap();
    std::fs::write(dir.path().join("hello.txt"), "cuda v100 benchmark results").unwrap();

    // 2. 啟動 IndexerService
    let config = IndexerConfig {
        root_paths: vec![dir.path().to_owned()],
        ..Default::default()
    };
    let service = IndexerService::start(config).await.unwrap();

    // 3. 等待 initial crawl 完成
    tokio::time::timeout(Duration::from_secs(30), async {
        loop {
            if matches!(*service.state.read().await, ServiceState::Watching) { break; }
            tokio::time::sleep(Duration::from_millis(100)).await;
        }
    }).await.unwrap();

    // 4. 搜尋並驗證
    let results = service.search("cuda v100").await.unwrap();
    assert!(!results.is_empty());
    assert!(results[0].file_path.ends_with("hello.txt"));
}
```

### 11.3 Benchmark（tools/bench/）

```rust
// tools/bench/src/metadata.rs — Criterion benchmark

fn bench_filename_search(c: &mut Criterion) {
    let store = setup_store_with_n_files(1_000_000);

    c.bench_function("filename_prefix_search_1M", |b| {
        b.iter(|| {
            let rt = tokio::runtime::Runtime::new().unwrap();
            rt.block_on(store.search_filename_prefix("report", 20)).unwrap()
        });
    });
}
criterion_group!(benches, bench_filename_search);
criterion_main!(benches);
```

### 11.4 效能驗收 KPI

| 指標 | 目標值 | 量測方式 |
|------|--------|---------|
| 檔名前綴搜尋 (1M 檔) | < 30 ms (p99) | Criterion + SQLite WAL |
| 全文查詢 | < 300 ms (p99) | Tantivy searcher benchmark |
| 初次索引吞吐 | > 500 files/sec (SSD) | smoke test 計時 |
| 背景 CPU 使用 | < 60% (可設定) | 系統監測 |
| 索引磁碟占用 | < 30% of 原始文件大小 | df 計算 |

---

## 12. Phase 0 PoC 工作清單

以下為第一個 Sprint 可立即開始的任務，每項約 1~3 天：

### Week 1–2（基礎建設）

- [ ] 建立 Cargo Workspace，確認 `cargo build` 全 crate 可過
- [ ] `rn-core`：實作 `IndexTask`, `DocumentState`, `ExtractResult`, `SearchResult`
- [ ] `rn-meta`：建立 SQLite schema + `MetaStore::open`, `is_up_to_date`, `search_filename_prefix`
- [ ] `rn-meta`：單元測試覆蓋 upsert / prefix search / state 轉換

### Week 3–4（Tantivy PoC）

- [ ] `rn-search`：建立 `RnSchema::build()`，能寫入與讀取一筆文件
- [ ] `rn-search`：實作 `JiebaTokenizer` 並確認中文可正確分詞
- [ ] `rn-search`：實作 `QueryParser::parse()` 支援 keyword + field 查詢
- [ ] `rn-search`：以 10 萬筆混合文件跑 Criterion benchmark，確認 < 300 ms

### Week 5–6（Indexer Pipeline PoC）

- [ ] `rn-extractors`：`PlainTextExtractor` + `PdfExtractor` + `DocxExtractor`
- [ ] `rn-indexer`：`TaskQueue` + `ExtractWorker` + `IndexWriterWorker` 三段 pipeline 可跑通
- [ ] `rn-indexer`：`Crawler` 能掃描目錄並排入 1 萬筆任務
- [ ] smoke test：端到端測試，txt + pdf + docx 檔可被索引並搜尋到

### Week 7–8（Windows + GPU PoC）

- [ ] `rn-windows`：Windows Service 安裝 / 啟動 / 停止 / 移除 可正常執行
- [ ] `rn-windows`：`FsWatcher` + `Debouncer` 能正確接收 create/modify/delete 事件
- [ ] `rn-gpu`：`NullBackend` + `CudaBackend::try_init()` 架構搭建，confirm fallback 機制
- [ ] CI：GitHub Actions Windows matrix build + 產出 MSI

### Phase 0 交付物

1. 可在 Windows 10/11 執行的 `rn-cli index` + `rn-cli search` MVP
2. `benchmark_report_phase0.md`：Tantivy 查詢 / Metadata 前綴搜尋 / 索引吞吐基準數據
3. `risk_report_phase0.md`：實測發現的技術風險與決策修正

---

*本文件為活文件，每 Sprint 末應更新一次。如有架構決策變更，請在對應章節加入 ADR（Architecture Decision Record）標記。*
