# Recoll for Windows — Promotional Content

Ready-to-post copy for various channels. Replace `[YOUR NAME]` and `[YOUR EMAIL]` where applicable.

---

## 1. Recoll mailing list (recoll-user@lists.recoll.org)

**Subject:** [ANN] Free Recoll Windows installer (1.43.13) — community build with Qt6 GUI

Hi all,

I have published a free, open source Windows installer for Recoll, built from the official upstream source code. This is intended to fill the gap for Windows users who want a direct, no-cost binary download.

**Download:** https://github.com/alarmz/recoll/releases/latest

**What is included:**
- recoll.exe — Qt6 GUI search interface
- recollindex.exe — file indexer
- recollq.exe — command-line query tool
- All required runtime DLLs bundled
- Inno Setup installer (~9 MB)

**Build details:**
- Built from Recoll 1.43.13 upstream source
- Toolchain: MSYS2 / MinGW-w64 (GCC 15.2)
- Qt 6.11
- Xapian 1.4.x
- Automated CI/CD via GitHub Actions on every release

**Installation options:**
- Direct installer download from GitHub Releases
- winget (PR pending): `winget install alarmz.Recoll`
- Scoop (PR pending): `scoop install extras/recoll`

The repository, build scripts, and installer source are all open and welcome contributions:
https://github.com/alarmz/recoll

Feedback and bug reports are very welcome.

Best regards,
[YOUR NAME]

---

## 2. Reddit — r/opensource

**Title:** Free Windows installer for Recoll (open source desktop search) — finally an easy install

Recoll is the most popular open source desktop full-text search tool on Linux. It indexes and searches PDF, Word, email, source code, and hundreds of other formats. The problem on Windows: until now, there has been no free, easy-to-install binary download. The only Windows version available cost money.

I built a free community installer using MSYS2/MinGW64 and Qt6, with automated GitHub Actions CI/CD.

- **Download:** https://github.com/alarmz/recoll/releases/latest
- **GitHub:** https://github.com/alarmz/recoll
- Coming soon: `winget install alarmz.Recoll` and `scoop install extras/recoll`

GPL-2.0, source code mirrors upstream Recoll. PRs welcome.

---

## 3. Reddit — r/software

**Title:** Recoll for Windows — free desktop full-text search (PDF, Word, email, code, more)

If you have ever wanted Spotlight-style desktop search on Windows that actually looks inside files, Recoll is the gold standard on Linux. The Windows version was previously only available as a paid product.

I built and published a free community installer:

- **Download:** https://github.com/alarmz/recoll/releases/latest
- 9 MB Qt6 GUI installer, 64-bit
- Searches PDF, DOCX/XLSX/PPTX, emails (EML/MBOX), HTML, source code, and many more formats
- Boolean queries, phrase search, wildcard, document preview
- 100% free and open source (GPL-2.0)

It's built directly from the upstream Recoll source via GitHub Actions, so the build is reproducible and auditable.

---

## 4. Reddit — r/windows

**Title:** Looking for "Spotlight for Windows" that searches inside files? Recoll now has a free installer

Windows Search is fine for filenames but limited for full-text content search across many formats. Recoll is a long-time open source champion in this space on Linux. I just published a free Windows installer:

https://github.com/alarmz/recoll/releases/latest

- Qt6 GUI, 64-bit, ~9 MB installer
- Indexes PDF, Office docs, emails, code, archives
- Free and open source (GPL)
- No telemetry, no ads, no paid tier

---

## 5. Hacker News (Show HN)

**Title:** Show HN: Free Windows installer for Recoll, the open source desktop search tool

Recoll has been the de-facto open source desktop full-text search tool on Linux for over 15 years. It indexes and searches inside PDF, Office documents, emails, source code, and ~200 other formats using Xapian under the hood.

Until now, the only way to get Recoll on Windows was either to compile it yourself (non-trivial — POSIX dependencies, Qt setup, Xapian build) or to buy a paid third-party package.

I spent the last few weeks porting the build to MSYS2/MinGW64 and Qt6, fixing a handful of POSIX-isms, and setting up GitHub Actions to produce reproducible installers automatically.

**Download:** https://github.com/alarmz/recoll/releases/latest
**Repo:** https://github.com/alarmz/recoll

The build is fully open: 4 small upstream patches (one missing #include, regex link flag, test subsystem fix, and an installer config), Qt6 GUI, ~9 MB Inno Setup installer. CI/CD runs on every release. Package manager submissions are in flight (winget, Scoop, Chocolatey).

I'd love feedback, bug reports, and PRs. Especially interested in:
- Real-world performance vs Windows Search
- Any document formats that fail to index (the Python filter ecosystem may need work on Windows)
- Whether anyone wants a portable .zip alongside the installer

---

## 6. AlternativeTo.net description

**Software name:** Recoll for Windows (community build)

**Tagline:** Free desktop full-text search for Windows — open source, no paid tier

**Description:**
Recoll is the most popular open source desktop full-text search tool on Linux, and now has a free community-maintained Windows installer. Indexes and searches the content of PDF, Word, Excel, PowerPoint, emails, HTML, source code, and hundreds of other formats. Powered by Xapian, with a Qt6 GUI, advanced query language, document preview, and zero telemetry.

**Categories:** File Search, Desktop Search, Document Management

**Tags:** open-source, free, full-text-search, desktop-search, gpl, qt, indexer, file-search

**Download URL:** https://github.com/alarmz/recoll/releases/latest

---

## 7. Tips for posting

- **Best times to post on Reddit:** Tuesday-Thursday, 9 AM EST or 6 PM EST
- **Hacker News:** Post on a weekday morning EST. Don't post on Friday/weekends.
- **r/opensource:** Always include "GPL" or license info in the body
- **Avoid:** Cross-posting to many subreddits at once (looks spammy). Pick 2-3 most relevant.
- **Engage:** Reply to every comment in the first 2 hours. Drives algorithm boost.
