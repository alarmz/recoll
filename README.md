# Recoll for Windows

[English](#english) | [中文](#中文)

---

## English

### Free Recoll Windows installer and binaries

This repository provides a free, community-maintained Windows build of Recoll, the open source desktop full-text search tool.

If you were searching for any of these, you are in the right place:

- Recoll Windows download
- Recoll Windows installer
- Recoll for Windows binary
- Recoll setup.exe
- Free Recoll Windows build

### Quick download

- [Download the latest Recoll Windows installer (64-bit)](https://github.com/alarmz/recoll/releases/latest)
- [Windows download page](docs/index.md)

### What this repository provides

- Free Windows installer for Recoll
- Prebuilt 64-bit Windows binaries
- Source code for the Windows build
- Winget manifest files for easier package distribution

This is intended to make Recoll easier to use on Microsoft Windows without requiring users to compile it themselves.

### What is Recoll?

Recoll is a desktop full-text search tool. It finds keywords inside documents as well as file names.

Main features:

- Available for Linux and Windows
- Searches many document formats
- Can search files, archive members, and email attachments
- Supports previewing and opening matching documents
- Free and open source software under the GPL

For more detail, see the [official features page](https://www.recoll.org/pages/features.html) and the [official documentation](https://www.recoll.org/pages/documentation.html).

### Install on Windows

#### Option 1: Download the installer

Download the latest installer from [GitHub Releases](https://github.com/alarmz/recoll/releases/latest) and run the `recoll-setup-*.exe` file.

#### Option 2: Install with `winget`

Once the package is published in the Windows Package Manager community repository, users will be able to install it with:

```powershell
winget install alarmz.Recoll
```

The manifest files for this are in [winget/](winget/).

#### Option 3: Install with `Scoop`

This repository also works as a Scoop bucket:

```powershell
scoop bucket add alarmz https://github.com/alarmz/recoll
scoop install alarmz/recoll
```

The manifest is in [bucket/recoll.json](bucket/recoll.json).

#### Option 4: Install with Chocolatey

A Chocolatey package skeleton is included in [packaging/chocolatey](packaging/chocolatey/). Publishing to the public Chocolatey community feed requires a maintainer account and package push.

### Install on Linux

Most Linux distributions already package Recoll. Typical examples:

```bash
apt install recoll
dnf install recoll
pacman -S recoll
```

If you need to build from source, see the [official build instructions](https://www.recoll.org/usermanual/usermanual.html#RCL.INSTALL.BUILDING).

### Windows build notes

- Platform: Windows x64
- Installer format: Inno Setup
- Current Qt build noted in this repository: Qt 6.8.2
- Windows-related build scripts live under [src/windows](src/windows/).

### Why this helps users

Many users can find Recoll on Linux package managers, but Windows users often search for:

- Recoll Windows binary
- Recoll Windows setup
- Recoll installer download
- Everything alternative with full-text search
- DocFetcher alternative

This repository exists to give those users a direct, free, open source download path.

### Search keywords

Recoll Windows, Recoll for Windows, Recoll Windows download, Recoll Windows installer, Recoll Windows binary, Recoll setup.exe, free Windows desktop search, open source desktop search, document full-text search, file content search, Everything alternative, DocFetcher alternative

---

## 中文

### 免費的 Recoll Windows 安裝程式與 binary

這個 repository 提供 Recoll 的免費、社群維護 Windows 版本，讓使用者不用自行編譯，也能在 Windows 上安裝使用這個開源桌面全文搜尋工具。

如果你是搜尋下面這些關鍵字找到這裡，這個 repo 就是你要的：

- Recoll Windows 下載
- Recoll Windows 安裝程式
- Recoll Windows binary
- Recoll setup.exe
- 免費 Recoll Windows 版本

### 快速下載

- [下載最新 Recoll Windows 安裝程式（64-bit）](https://github.com/alarmz/recoll/releases/latest)
- [Windows 下載說明頁](docs/index.md)

### 這個 repo 提供什麼

- 免費的 Recoll Windows 安裝程式
- 預先編譯好的 64-bit Windows binary
- 可對照的原始碼
- 可用於發佈的 winget manifest

這個 fork 的目的，是讓 Windows 使用者可以直接安裝 Recoll，而不是卡在自行編譯環境。

### Recoll 是什麼？

Recoll 是桌面全文搜尋工具，可搜尋文件內容與檔案名稱。

主要特色：

- 支援 Linux 與 Windows
- 可搜尋多種文件格式
- 可搜尋檔案、壓縮檔內容與電子郵件附件
- 支援預覽與開啟命中的文件
- 免費、開源，採 GPL 授權

更多資訊可參考 [官方功能頁](https://www.recoll.org/pages/features.html) 與 [官方文件](https://www.recoll.org/pages/documentation.html)。

### Windows 安裝方式

#### 方式 1：直接下載安裝程式

到 [GitHub Releases](https://github.com/alarmz/recoll/releases/latest) 下載最新的 `recoll-setup-*.exe` 後直接執行。

#### 方式 2：使用 `winget`

等套件發佈到 Windows Package Manager 社群倉庫後，使用者可以直接執行：

```powershell
winget install alarmz.Recoll
```

相關 manifest 已經放在 [winget/](winget/)。

#### 方式 3：使用 `Scoop`

這個 repository 也可以直接當成 Scoop bucket 使用：

```powershell
scoop bucket add alarmz https://github.com/alarmz/recoll
scoop install alarmz/recoll
```

manifest 位於 [bucket/recoll.json](bucket/recoll.json)。

#### 方式 4：使用 Chocolatey

此 repo 已附上 [packaging/chocolatey](packaging/chocolatey/) 的 Chocolatey 套件骨架；若要發佈到公開 Chocolatey community feed，還需要 maintainer 帳號與 push 權限。

### Linux 安裝方式

大多數 Linux 發行版都已內建 Recoll 套件，例如：

```bash
apt install recoll
dnf install recoll
pacman -S recoll
```

如果你需要自行編譯，可參考 [官方 build 說明](https://www.recoll.org/usermanual/usermanual.html#RCL.INSTALL.BUILDING)。

### Windows build 補充

- 平台：Windows x64
- 安裝程式格式：Inno Setup
- 目前此 repo 記錄的 Qt 版本：Qt 6.8.2
- Windows 相關 build script 位於 [src/windows](src/windows/)。

### 為什麼這個 repo 有價值

Linux 使用者通常可以直接從套件管理器安裝，但 Windows 使用者常常會搜尋：

- Recoll Windows binary
- Recoll Windows setup
- Recoll Windows installer
- 全文搜尋 Windows 工具
- Everything 替代品
- DocFetcher 替代品

這個 repository 的價值，就是提供一個直接、免費、開源的 Windows 下載入口。

### 搜尋關鍵字

Recoll Windows, Recoll for Windows, Recoll Windows 下載, Recoll Windows 安裝檔, Recoll Windows binary, Recoll setup.exe, 免費 Windows 搜尋工具, 開源桌面搜尋, 文件全文搜尋, 檔案內容搜尋, Everything 替代品, DocFetcher 替代品
