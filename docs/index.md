---
title: "Recoll for Windows — Free Download"
description: "Download Recoll for Windows for free. Full-text desktop search with Qt6 GUI installer. Search PDF, Word, email and hundreds of file formats on Windows."
---

# Recoll for Windows — Free Download

The free, open source desktop full-text search tool — now with a Windows installer.

## Download

**[Download Recoll Windows installer (64-bit)](https://github.com/alarmz/recoll/releases/latest)**

Includes:
- `recoll.exe` — Qt6 GUI search interface
- `recollindex.exe` — file indexer
- `recollq.exe` — command-line query tool

## What is Recoll?

Recoll is the most popular open source desktop full-text search tool on Linux. It indexes and searches your files — PDFs, Word documents, emails, source code, and hundreds of other formats.

**Key features:**
- Searches inside documents, not just file names
- Supports PDF, DOCX, XLSX, PPTX, emails, HTML, source code, and more
- Fast full-text search powered by Xapian
- Advanced query language with boolean operators
- Preview and open matching documents directly
- Free and open source (GPL)

## Install

### Option 1: Download installer
1. Download [`recoll-1.43.13-win64-setup.exe`](https://github.com/alarmz/recoll/releases/latest)
2. Run the installer
3. Start Recoll from the Start menu

### Option 2: winget
```
winget install alarmz.Recoll
```

### Option 3: Scoop
```
scoop bucket add recoll https://github.com/alarmz/recoll
scoop install recoll
```

## Why this exists

Recoll is extremely popular on Linux but has no official free Windows binary download. The only Windows version previously available required payment.

This community-maintained fork provides:
- Free Windows installer built from the official open source code
- Automated CI/CD builds using MSYS2/MinGW64 and Qt6
- Package manager support (winget, Scoop)

## Links

- [Source code (GitHub)](https://github.com/alarmz/recoll)
- [Official Recoll website](https://www.recoll.org/)
- [Recoll documentation](https://www.recoll.org/pages/documentation.html)
- [Report issues](https://github.com/alarmz/recoll/issues)
