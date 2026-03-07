# Recoll

Recoll is a desktop full-text search tool. It finds keywords inside
documents as well as file names.

* **Cross-platform**: native builds for Linux and Windows (MSVC).
* A WEB front-end with preview and download features can replace or
  supplement the GUI for remote use.
* It can search most document formats. You may need external applications
  for text extraction.
* It can reach any storage place: files, archive members, email
  attachments, transparently handling decompression.
* One click will open the document inside a native editor or display an
  even quicker text preview.
* The software is free, open source, and licensed under the GPL.

For more detail, see the [features page on the web site](https://www.recoll.org/pages/features.html) or
the [online documentation](https://www.recoll.org/pages/documentation.html).

## Windows Support

Recoll now builds natively on Windows using MSVC and CMake. The Windows
build produces `recollindex.exe` and `recollq.exe` for command-line
indexing and querying.

### Building on Windows

**Prerequisites:**
- Visual Studio 2022 Build Tools (MSVC)
- CMake 3.25+
- Ninja
- [vcpkg](https://github.com/microsoft/vcpkg)

**Install dependencies:**
```powershell
vcpkg install xapian:x64-windows libxml2:x64-windows libxslt:x64-windows zlib:x64-windows libiconv:x64-windows
```

**Build:**
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

cd src
cmake -S . -B build_win -G Ninja ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DRECOLL_QTGUI=OFF ^
  -DXAPIAN_SHARED=OFF

cmake --build build_win --parallel
```

**Run tests:**
```cmd
cd build_win
ctest --output-on-failure
```

### Windows Installer

A Windows installer is available for each release. Download
`recoll-<version>-win64-setup.exe` from the
[Releases](../../releases) page. The installer adds Recoll to your
PATH so you can use `recollindex` and `recollq` from any terminal.

## Building on Linux

Most distributions feature prebuilt packages for Recoll. To build from
source:

```bash
sudo apt-get install cmake ninja-build \
  libxapian-dev libxml2-dev libxslt1-dev zlib1g-dev \
  qtbase5-dev qtwebengine5-dev qttools5-dev

cd src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

For detailed build instructions, see the
[online documentation](https://www.recoll.org/usermanual/usermanual.html#RCL.INSTALL.BUILDING).

## Recoll Next (Rust Rewrite)

A next-generation rewrite of Recoll in Rust using Tantivy as the search
engine. The Rust components live under the `crates/` directory:

- **rn-core**: Core types — IndexTask, DocumentState, ExtractResult, SearchResult, etc.
- **rn-meta**: SQLite metadata store (rusqlite, WAL mode)
- **rn-search**: Tantivy search engine — schema, tokenizers, query parsing, ranking, snippets
- **rn-extractors**: Document extractors — plain text, HTML, Markdown, source code, CSV, fallback
- **rn-indexer**: Indexer pipeline — task queue, crawler, throttle, commit policy, text normalization, service state
- **rn-watcher**: File system watcher — debouncer, event mapper, reconciler, FsWatcher (notify)
- **rn-cli**: CLI tool — clap commands (init, search, index, stats, doctor), config, output formatting
- **rn-windows**: Windows integration — service config, installer args, shell extension, cancellation token, status
- **rn-gpu**: GPU acceleration — GpuBackend trait, NullBackend fallback, GpuDispatcher, factory
- **rn-sdk**: SDK / Public API — RecollNext handle, SearchRequest, HealthReport, endpoints, FFI types
- **rn-gui**: GUI desktop app — SearchViewModel, FilterState, PreviewData, StatusInfo, TrayAction
- **rn-installer**: Installer — InstallerManifest, WixConfig, InnoConfig, VersionInfo, ReleaseInfo
- **rn-logging**: Logging, monitoring & security — LogConfig, Rotation, SecurityPolicy, HealthCheck, AuditEntry
- **rn-bench**: Performance & benchmarking — KpiThresholds, BenchConfig, ResourceLimits, BenchmarkReport, StressConfig

### Building & Testing (Rust)

```bash
# Run all Rust tests (299 tests across rn-core, rn-meta, rn-search, rn-extractors, rn-indexer, rn-watcher, rn-cli, rn-windows, rn-gpu, rn-sdk, rn-gui, rn-installer, rn-logging, rn-bench)
cargo test --all

# Check formatting and lint
cargo fmt --all -- --check
cargo clippy --all-targets -- -D warnings
```

See [docs/todo.md](docs/todo.md) for the full implementation roadmap.

## CI/CD

This project uses GitHub Actions for continuous integration:

- **Rust CI** (`ci.yml`): Runs `cargo fmt`, `cargo clippy -D warnings`,
  and `cargo test --all` on Ubuntu + Windows matrix for every push and
  pull request.
- **C++ CI** (`ci.yml`): Builds the C++ codebase with CMake/Ninja and
  runs CTest on Linux and Windows.
- **Release** (`release.yml`): Builds Linux and Windows binaries and
  a Windows installer on each GitHub Release.

---

# Recoll (中文)

Recoll 是一個桌面全文搜尋工具，可以在文件內容和檔名中搜尋關鍵字。

* **跨平台支援**：原生支援 Linux 和 Windows (MSVC) 編譯。
* 提供 WEB 前端介面，支援預覽和下載功能，可用於遠端存取。
* 支援大多數文件格式的搜尋，部分格式需要外部應用程式進行文字擷取。
* 可搜尋各種儲存位置：檔案、壓縮檔成員、電子郵件附件，自動處理解壓縮。
* 一鍵即可在原生編輯器中開啟文件，或快速顯示文字預覽。
* 本軟體為自由開源軟體，採用 GPL 授權。

## Windows 支援

Recoll 現在可以在 Windows 上使用 MSVC 和 CMake 原生編譯。Windows 版本會產生
`recollindex.exe` 和 `recollq.exe`，用於命令列索引和查詢。

### 在 Windows 上編譯

**必要條件：**
- Visual Studio 2022 Build Tools (MSVC)
- CMake 3.25+
- Ninja
- [vcpkg](https://github.com/microsoft/vcpkg)

**安裝相依套件：**
```powershell
vcpkg install xapian:x64-windows libxml2:x64-windows libxslt:x64-windows zlib:x64-windows libiconv:x64-windows
```

**編譯：**
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64

cd src
cmake -S . -B build_win -G Ninja ^
  -DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DRECOLL_QTGUI=OFF ^
  -DXAPIAN_SHARED=OFF

cmake --build build_win --parallel
```

**執行測試：**
```cmd
cd build_win
ctest --output-on-failure
```

### Windows 安裝程式

每個版本都會提供 Windows 安裝程式。請從
[Releases](../../releases) 頁面下載
`recoll-<version>-win64-setup.exe`。安裝程式會將 Recoll 加入系統 PATH，
讓你可以在任何終端機中使用 `recollindex` 和 `recollq`。

## 在 Linux 上編譯

大多數 Linux 發行版都有預編譯的 Recoll 套件。如需從原始碼編譯：

```bash
sudo apt-get install cmake ninja-build \
  libxapian-dev libxml2-dev libxslt1-dev zlib1g-dev \
  qtbase5-dev qtwebengine5-dev qttools5-dev

cd src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

詳細的編譯說明請參閱
[線上文件](https://www.recoll.org/usermanual/usermanual.html#RCL.INSTALL.BUILDING)。

## Recoll Next (Rust 重寫)

使用 Rust 和 Tantivy 搜尋引擎重寫的下一代 Recoll。Rust 元件位於 `crates/` 目錄下：

- **rn-core**：核心型別 — IndexTask、DocumentState、ExtractResult、SearchResult 等
- **rn-meta**：SQLite metadata 儲存 (rusqlite, WAL mode)
- **rn-search**：Tantivy 搜尋引擎 — schema、tokenizer、查詢解析、排序、snippet
- **rn-extractors**：文件抽取器 — 純文字、HTML、Markdown、原始碼、CSV、fallback
- **rn-indexer**：索引管線 — 任務佇列、檔案爬取、節流、提交策略、文字正規化、服務狀態
- **rn-watcher**：檔案監控 — 事件去抖動、事件轉換、檔案校正、FsWatcher (notify)
- **rn-cli**：CLI 工具 — clap 命令 (init、search、index、stats、doctor)、設定、輸出格式化
- **rn-windows**：Windows 整合 — 服務設定、安裝命令、Shell Extension、取消令牌、狀態解析
- **rn-gpu**：GPU 加速 — GpuBackend trait、NullBackend fallback、GpuDispatcher、factory
- **rn-sdk**：SDK / Public API — RecollNext handle、SearchRequest、HealthReport、端點定義、FFI 型別
- **rn-gui**：GUI 桌面應用 — SearchViewModel、FilterState、PreviewData、StatusInfo、TrayAction
- **rn-installer**：安裝器 — InstallerManifest、WixConfig、InnoConfig、VersionInfo、ReleaseInfo
- **rn-logging**：日誌/監控/安全 — LogConfig、Rotation、SecurityPolicy、HealthCheck、AuditEntry
- **rn-bench**：效能基準測試 — KpiThresholds、BenchConfig、ResourceLimits、BenchmarkReport、StressConfig

### 編譯與測試 (Rust)

```bash
# 執行所有 Rust 測試 (共 299 個測試，涵蓋 rn-core、rn-meta、rn-search、rn-extractors、rn-indexer、rn-watcher、rn-cli、rn-windows、rn-gpu、rn-sdk、rn-gui、rn-installer、rn-logging、rn-bench)
cargo test --all

# 檢查格式和 lint
cargo fmt --all -- --check
cargo clippy --all-targets -- -D warnings
```

詳細的實作進度請參閱 [docs/todo.md](docs/todo.md)。

## CI/CD

本專案使用 GitHub Actions 進行持續整合：

- **Rust CI** (`ci.yml`)：每次 push 和 pull request 都會在 Ubuntu + Windows 矩陣上執行 `cargo fmt`、`cargo clippy -D warnings` 和 `cargo test --all`。
- **C++ CI** (`ci.yml`)：使用 CMake/Ninja 編譯 C++ 程式碼，並在 Linux 和 Windows 上執行 CTest。
- **Release** (`release.yml`)：每次在 GitHub 發布 Release 時，會自動編譯 Linux 和 Windows 的二進位檔案，並產生 Windows 安裝程式。
