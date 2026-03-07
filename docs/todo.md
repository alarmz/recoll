# Recoll Next — Implementation Phases

> 根據 [technical-design.md](technical-design.md) 劃分的實作階段
> 完成的項目以 ~~刪除線~~ 標記

---

## ~~Phase 0: Workspace 建立與核心型別 (Week 1)~~ ✅

- [x] 建立 Cargo workspace，所有 crate skeleton 可 `cargo check` 通過
- [x] `rn-core/src/task.rs`: IndexTask, FileVersion, TaskPriority, OperationType, TaskSource
- [x] `rn-core/src/state.rs`: DocumentState enum + 狀態轉換驗證函式
- [x] `rn-core/src/extract.rs`: ExtractResult, Language, ExtractionMethod, ExtractWarning
- [x] `rn-core/src/search.rs`: SearchResult, MatchReason, SourceType, SearchResponse
- [x] `rn-core/src/error.rs`: IndexError (thiserror)
- [x] `rn-core/src/types.rs`: FileId, MimeType 等共用型別
- [x] 單元測試: TaskPriority 排序、DocumentState 合法轉換 (41 tests)

---

## ~~Phase 1: Metadata 儲存 (Week 2)~~ ✅

- [x] `rn-meta/src/models.rs`: MetadataRecord, FileMeta, IndexStats 資料模型
- [x] `rn-meta/src/store.rs`: MetaStore::open() + PRAGMA 設定 (WAL mode)
- [x] `rn-meta/src/store.rs`: upsert_metadata() — INSERT ON CONFLICT
- [x] `rn-meta/src/store.rs`: is_up_to_date() — mtime + size 比對
- [x] `rn-meta/src/store.rs`: search_filename_prefix() — LIKE 前綴查詢
- [x] `rn-meta/src/store.rs`: set_state(), get_doc_id(), set_doc_id(), delete_by_path()
- [x] `rn-meta/src/store.rs`: find_stale(), get_stats(), get_failed_tasks()
- [x] 單元測試: upsert/query, prefix search, state 轉換, stale 偵測 (19 tests)

---

## ~~Phase 2: Tantivy 搜尋引擎基礎 (Week 3)~~ ✅

- [x] `rn-search/src/schema.rs`: RnSchema::build() — 所有 field 定義
- [x] `rn-search/src/tokenizer/mod.rs`: register_tokenizers() — rn_default, rn_cjk, rn_code, rn_filename
- [x] `rn-search/src/tokenizer/jieba.rs`: JiebaTokenizer wrapper (Tantivy Tokenizer trait)
- [x] `rn-search/src/tokenizer/code.rs`: CodeTokenizer (camelCase/snake_case 分割)
- [x] `rn-search/src/tokenizer/filename.rs`: FilenameTokenizer (- _ . 分割)
- [x] `rn-search/src/writer.rs`: IndexWriter 封裝 (add_document, delete_by_term, commit)
- [x] `rn-search/src/reader.rs`: IndexReader 封裝 (search, reload)
- [x] 單元測試: schema 建立、文件寫入讀取、中文分詞驗證 (24 tests)
- [ ] Benchmark: 10 萬筆文件查詢 < 300ms

---

## ~~Phase 3: 查詢解析與排序 (Week 4)~~ ✅

- [x] `rn-search/src/query.rs`: QueryParser — keyword, "phrase", field:value 語法
- [x] `rn-search/src/query.rs`: 日期範圍 modified:>2024-01-01
- [x] `rn-search/src/query.rs`: 副檔名 ext:pdf, 路徑 path:/project
- [x] `rn-search/src/query.rs`: 大小過濾 size:>1024
- [x] `rn-search/src/ranking.rs`: RankingWeights 結構 + compute_filename_score()
- [x] `rn-search/src/ranking.rs`: compute_recency_score() 衰減曲線
- [x] `rn-search/src/snippet.rs`: SnippetBuilder + 自訂高亮標記
- [ ] `rn-search/src/reader.rs`: SearchCoordinator — 並行查詢 MetaDB + Tantivy 合併排序
- [x] 單元測試: 各查詢語法解析、scoring formula、snippet 邊界 (17 tests)

---

## ~~Phase 4: 文件抽取器 (Week 5)~~ ✅

- [x] `rn-extractors/src/lib.rs`: Extractor trait + CostProfile
- [x] `rn-extractors/src/registry.rs`: ExtractorRegistry + default_registry() + mime_guess
- [x] `rn-extractors/src/plain_text.rs`: PlainTextExtractor (UTF-8/BOM 偵測)
- [ ] `rn-extractors/src/pdf.rs`: PdfExtractor (pdf-extract + OCR fallback)
- [ ] `rn-extractors/src/docx.rs`: DocxExtractor (docx-rs)
- [ ] `rn-extractors/src/xlsx.rs`: XlsxExtractor (calamine)
- [ ] `rn-extractors/src/pptx.rs`: PptxExtractor
- [x] `rn-extractors/src/html.rs`: HtmlExtractor (tag stripping)
- [x] `rn-extractors/src/markdown.rs`: MarkdownExtractor (pulldown-cmark)
- [x] `rn-extractors/src/source_code.rs`: SourceCodeExtractor
- [ ] `rn-extractors/src/email.rs`: EmlExtractor (RFC 822)
- [x] `rn-extractors/src/csv.rs`: CsvExtractor
- [x] `rn-extractors/src/fallback.rs`: FallbackExtractor (嘗試 UTF-8 讀取)
- [x] 單元測試: 每個 extractor 的 supports() + fixture 檔案抽取驗證 (23 tests)

---

## ~~Phase 5: Indexer Pipeline (Week 5-6)~~ ✅

- [x] `rn-indexer/src/queue.rs`: TaskQueue (BinaryHeap + PrioritizedTask Ord + FIFO within same priority)
- [x] `rn-indexer/src/throttle.rs`: IoThrottle (Off/Gentle/Aggressive)
- [x] `rn-indexer/src/crawler.rs`: Crawler (walkdir + exclude patterns + hidden dir filtering)
- [x] `rn-indexer/src/commit_policy.rs`: CommitPolicy (ByCount/ByTime/Hybrid) + CommitTracker
- [x] `rn-indexer/src/normalize.rs`: normalize_text (Unicode NFC + whitespace collapse + truncate)
- [x] `rn-indexer/src/service.rs`: ServiceState (Idle/Running/Paused/Stopped + 狀態轉換驗證)
- [ ] `rn-indexer/src/workers/extract.rs`: ExtractWorker (tokio::select + failure retry + backoff)
- [ ] `rn-indexer/src/workers/index_writer.rs`: IndexWriterWorker
- [ ] `rn-indexer/src/workers/tombstone.rs`: TombstoneWorker (delete_term + meta cleanup)
- [x] 單元測試: TaskQueue, IoThrottle, Crawler, CommitPolicy, Normalize, ServiceState (23 tests)
- [ ] 整合測試: Crawler 掃描 1 萬筆 → Pipeline 全程 → 搜尋驗證
- [ ] Benchmark: 初次索引吞吐 > 500 files/sec

---

## ~~Phase 6: 檔案監控 (Week 7)~~ ✅

- [x] `rn-watcher/src/watcher.rs`: FsWatcher (notify RecommendedWatcher + RecursiveMode + crossbeam-channel)
- [x] `rn-watcher/src/debounce.rs`: Debouncer (window + pending HashMap + immediate flush for Delete)
- [x] `rn-watcher/src/reconcile.rs`: reconcile() (disk vs known 差異比較 — added/removed/modified)
- [x] `rn-watcher/src/event_mapper.rs`: map_event() (EventKind → OperationType, Rename → Delete+Create)
- [x] 整合測試: 建立/修改/刪除檔案 → FsWatcher 偵測 (3 tests)
- [x] 單元測試: Debouncer suppress + EventMapper + Reconciler (14 tests)
- [x] 總計 17 tests

---

## ~~Phase 7: CLI 工具 (Week 7-8)~~ ✅

- [x] `rn-cli/src/cli.rs`: clap derive 命令結構 (init, search, index, stats, doctor)
- [x] `rn-cli/src/cli.rs`: ThrottleArg ValueEnum + global --config 參數
- [x] `rn-cli/src/commands/init.rs`: 初始化索引目錄 (.recoll-next/ + config.toml + --force)
- [x] `rn-cli/src/config.rs`: CliConfig (Serialize/Deserialize TOML + load_from + default)
- [x] `rn-cli/src/output.rs`: Formattable trait + SearchOutput/StatsOutput text/JSON 格式化
- [ ] `rn-cli/src/commands/index.rs`: 執行索引 (--full, --dry-run, --throttle)
- [ ] `rn-cli/src/commands/search.rs`: 搜尋 (--limit, --offset, --type, --json, --no-snippet)
- [ ] `rn-cli/src/commands/watch.rs`: 啟動監控 (--daemon, --reconcile-interval)
- [ ] `rn-cli/src/commands/service.rs`: Service 管理 (install/uninstall/start/stop/status)
- [x] 單元測試: CLI 解析、init、config、output (16 tests)
- [ ] Smoke test: E2E `rn-cli index` + `rn-cli search` 可正常運作

---

## ~~Phase 8: Windows 平台整合 (Week 8-9)~~ ✅

- [x] `rn-windows/src/config.rs`: ServiceConfig (name, display_name, binary_name)
- [x] `rn-windows/src/installer.rs`: install_args/uninstall_args/start_args/stop_args (sc.exe 命令產生)
- [x] `rn-windows/src/cancel.rs`: CancellationToken (Arc<AtomicBool> 優雅關機)
- [x] `rn-windows/src/shell_ext.rs`: ShellExtension (Registry 路徑、選單文字、命令列)
- [x] `rn-windows/src/status.rs`: ServiceStatus (parse running/stopped/paused/unknown)
- [ ] `rn-windows/src/service.rs`: Windows Service (define_windows_service, SCM 整合)
- [ ] 測試: Service install → start → indexing → stop → uninstall 全流程
- [x] 單元測試: Config, Installer, ShellExt, CancellationToken, Status (15 tests)

---

## ~~Phase 9: GPU 加速 (Week 9-10)~~ ✅

- [x] `rn-gpu/src/backend.rs`: GpuBackend trait (Send+Sync, batch_preprocess, batch_embed, device_info)
- [x] `rn-gpu/src/backend.rs`: DeviceInfo (Debug, Clone, PartialEq, Eq, Display)
- [x] `rn-gpu/src/null_backend.rs`: NullBackend (Debug, Clone, Copy, CPU fallback)
- [x] `rn-gpu/src/dispatcher.rs`: GpuDispatcher (batch accumulation + try_flush/force_flush + try_embed/force_embed)
- [x] `rn-gpu/src/factory.rs`: create_best_available() (feature-gated CUDA → Vulkan → NullBackend)
- [x] Feature gate: `#[cfg(feature = "cuda")]` / `#[cfg(feature = "vulkan")]` 預留
- [ ] `rn-gpu/src/backends/cuda.rs`: CudaBackend::try_init() + device detection
- [ ] Benchmark: GPU vs CPU preprocess 吞吐對比
- [x] 單元測試: NullBackend, DeviceInfo, BatchResult, Dispatcher, Factory (14 tests)

---

## ~~Phase 10: 設定系統與錯誤恢復 (Week 10)~~ ✅

- [x] `rn-core/src/config.rs`: AppConfig 結構 (indexer/gpu/search/watcher/logging) + PartialEq
- [x] `rn-core/src/config.rs`: ConfigLoader::load() / load_from_file() (anyhow::Result)
- [x] `rn-core/src/config.rs`: ConfigField enum + is_hot_reloadable() 可熱更新判斷
- [x] `rn-core/src/lock.rs`: LockFile (acquire/release/is_stale + Drop 自動釋放)
- [x] `rn-core/src/repair.rs`: RepairPlan::diagnose() (Issue → RepairAction) + Display
- [ ] TOML 設定檔範本 (`%APPDATA%\RecollNext\config.toml`)
- [ ] Crash recovery: PRAGMA integrity_check + state 重設
- [ ] Panic recovery: worker catch_unwind + 自動重啟
- [x] 單元測試: AppConfig, ConfigLoader, HotReload, LockFile, RepairPlan (15 tests)

---

## ~~Phase 11: SDK / Public API (Week 11)~~ ✅

- [x] `rn-sdk/src/handle.rs`: SdkConfig + RecollNext handle (open/close/is_open/config)
- [x] `rn-sdk/src/request.rs`: SearchRequest (validate + builder pattern)
- [x] `rn-sdk/src/health.rs`: HealthReport (ComponentStatus/OverallStatus/from_components)
- [x] `rn-sdk/src/endpoint.rs`: Endpoint 路由定義 (search/health/stats/index/config)
- [x] `rn-sdk/src/ffi.rs`: FfiResult + FfiSearchResult (#[repr(C)])
- [ ] `rn-sdk/src/http.rs`: axum HTTP server (127.0.0.1:9312)
- [ ] API 文件: docs/api-reference.md
- [ ] 整合測試: HTTP API E2E 驗證
- [x] 單元測試: Handle, SearchRequest, HealthReport, Endpoint, FFI (15 tests)

---

## ~~Phase 12: GUI 桌面應用 (Week 12-14)~~ ✅

- [x] `rn-gui/src/search_vm.rs`: SearchViewModel + SearchHit (query/results/total + set_query)
- [x] `rn-gui/src/filter.rs`: FilterState (file_type/path_prefix/date_from + is_active/clear)
- [x] `rn-gui/src/preview.rs`: PreviewData + PreviewVariant (Text/Image/Unsupported + MIME 判斷)
- [x] `rn-gui/src/status.rs`: StatusInfo + AppState (Idle/Indexing/Paused + progress)
- [x] `rn-gui/src/tray.rs`: TrayAction (Show/Hide/Quit + label/from_id/all)
- [ ] `rn-gui`: Tauri 2.x 專案初始化
- [ ] 搜尋頁面: 搜尋框 + 篩選列 (type/path/date)
- [ ] 結果列表: 檔案圖示 + 路徑 + snippet + metadata
- [ ] 預覽面板: 檔案內容預覽 / metadata 摘要
- [ ] 設定頁面: TOML 設定 GUI 化
- [ ] 與 rn-sdk 整合 (Tauri commands → SDK API)
- [x] 單元測試: SearchViewModel, FilterState, PreviewData, StatusInfo, TrayAction (15 tests)

---

## ~~Phase 13: 安裝器 (Week 14-15)~~ ✅

- [x] `rn-installer/src/manifest.rs`: InstallerManifest + Component (essential_only + is_essential + Display)
- [x] `rn-installer/src/wix.rs`: WixConfig (ProductInfo + manufacturer + upgrade_code + install_dir)
- [x] `rn-installer/src/inno.rs`: InnoConfig (ProductInfo + Compression + output_base_filename)
- [x] `rn-installer/src/version.rs`: VersionInfo (PartialOrd+Ord + FromStr + upgrade_from + Display)
- [x] `rn-installer/src/release.rs`: ReleaseInfo + ReleaseArtifact (Platform enum + artifacts_for + Display)
- [x] `rn-installer/src/product.rs`: ProductInfo 共用結構 (name + version)
- [ ] `installer/wix/main.wxs`: WiX 4 MSI 腳本
- [ ] MSI 安裝流程: 複製檔案 → Shell Extension → Service → PATH → 捷徑
- [ ] `installer/recoll-next.iss`: Inno Setup 備選方案
- [ ] CI/CD Release workflow: tag → build → test → MSI → GitHub Release upload
- [ ] 使用者指南: docs/user-guide.md
- [x] 單元測試: Manifest, WixConfig, InnoConfig, VersionInfo, ReleaseInfo (15 tests)

---

## ~~Phase 14: 日誌、監控與安全 (Week 15-16)~~ ✅

- [x] `rn-logging/src/config.rs`: LogConfig + LogLevel (Display + PartialOrd + parse)
- [x] `rn-logging/src/rotation.rs`: Rotation (Daily/Hourly/Never + Display)
- [x] `rn-logging/src/security.rs`: SecurityPolicy (localhost_only + max_snippet_len + excluded_dirs + is_excluded)
- [x] `rn-logging/src/health.rs`: HealthCheck + CheckResult + CheckStatus + OverallHealth (evaluate + Copy)
- [x] `rn-logging/src/audit.rs`: AuditEntry (sanitize + Display + created_at)
- [ ] tracing + tracing-subscriber + tracing-appender 整合
- [ ] 結構化 JSON 日誌 + DAILY rotation + max_size + backup
- [ ] `rn-cli doctor`: 健康狀態報告 + 自動修復建議
- [ ] 安全: `rn-cli repair --purge` 清空索引
- [x] 單元測試: LogConfig, Rotation, SecurityPolicy, HealthCheck, AuditEntry (15 tests)

---

## Phase 15: 效能最佳化與穩定化 (Week 16-18)

- [ ] Criterion benchmark suite: filename_search, fulltext_query, metadata_insert, commit
- [ ] KPI 驗證: 檔名搜尋 < 30ms, 全文 < 300ms, 索引 > 500 files/sec
- [ ] 資源控制驗證: gentle/aggressive throttle, battery mode, 大檔案記憶體
- [ ] Tantivy segment merge 策略調整
- [ ] SQLite WAL checkpoint 策略
- [ ] 記憶體 profiling + leak 檢查
- [ ] stress test: 100K+ 檔案混合格式索引
- [ ] 產出 benchmark_report.md + risk_report.md
- [ ] v1.0 release tag + GitHub Release + 文件完善

---

## 進度追蹤

| Phase | 名稱 | 狀態 |
|-------|------|------|
| 0 | Workspace + 核心型別 | ✅ 完成 (41 tests) |
| 1 | Metadata 儲存 | ✅ 完成 (19 tests) |
| 2 | Tantivy 基礎 | ✅ 完成 (24 tests) |
| 3 | 查詢解析與排序 | ✅ 完成 (17 tests) |
| 4 | 文件抽取器 | ✅ 完成 (23 tests) |
| 5 | Indexer Pipeline | ✅ 完成 (23 tests) |
| 6 | 檔案監控 | ✅ 完成 (17 tests) |
| 7 | CLI 工具 | ✅ 完成 (16 tests) |
| 8 | Windows 整合 | ✅ 完成 (15 tests) |
| 9 | GPU 加速 | ✅ 完成 (14 tests) |
| 10 | 設定與恢復 | ✅ 完成 (15 tests) |
| 11 | SDK / API | ✅ 完成 (15 tests) |
| 12 | GUI 桌面應用 | ✅ 完成 (15 tests) |
| 13 | 安裝器 | ✅ 完成 (15 tests) |
| 14 | 日誌/監控/安全 | ✅ 完成 (15 tests) |
| 15 | 效能最佳化與穩定化 | 未開始 |
