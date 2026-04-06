# Releasing Recoll for Windows

This repository can act as:

- a GitHub Pages landing site
- a Scoop bucket
- the source of truth for `winget` and Chocolatey manifests

## GitHub repository settings

Set these once in the GitHub web UI:

- Description: `Free Recoll Windows installer and binaries`
- Website: `https://alarmz.github.io/recoll/`

## GitHub Pages

In `Settings -> Pages`:

1. Set `Source` to `Deploy from a branch`.
2. Choose branch `master`.
3. Choose folder `/docs`.
4. Save.

The landing page will be published from [docs/index.md](/home/alarm/recoll/docs/index.md).

## Update package manifests after each release

Run:

```bash
./packaging/windows/update-package-manifests.sh
```

Important:

- The GitHub release tag should match the Windows installer version.
- If [src/windows/recoll-setup.iss](/home/alarm/recoll/src/windows/recoll-setup.iss) says `1.43.13`, the release tag should be `v1.43.13`.
- Do not publish package-manager manifests from a placeholder tag such as `Next`.

This updates:

- [recoll.json](/home/alarm/recoll/recoll.json)
- [bucket/recoll.json](/home/alarm/recoll/bucket/recoll.json)
- [winget/alarmz.Recoll.yaml](/home/alarm/recoll/winget/alarmz.Recoll.yaml)
- [winget/alarmz.Recoll.installer.yaml](/home/alarm/recoll/winget/alarmz.Recoll.installer.yaml)
- [winget/alarmz.Recoll.locale.en-US.yaml](/home/alarm/recoll/winget/alarmz.Recoll.locale.en-US.yaml)
- [packaging/chocolatey/recoll.install.nuspec](/home/alarm/recoll/packaging/chocolatey/recoll.install.nuspec)
- [packaging/chocolatey/tools/chocolateyinstall.ps1](/home/alarm/recoll/packaging/chocolatey/tools/chocolateyinstall.ps1)
- [packaging/chocolatey/tools/VERIFICATION.txt](/home/alarm/recoll/packaging/chocolatey/tools/VERIFICATION.txt)

## Release notes checklist

For each GitHub release, include:

- Tag name matching the installer version, for example `v1.43.13`
- Installer download link
- SHA256 checksum
- Version number
- What changed
- Known limitations

## Scoop

This repository already contains a bucket manifest at [bucket/recoll.json](/home/alarm/recoll/bucket/recoll.json).

Users can install from your own bucket with:

```powershell
scoop bucket add alarmz https://github.com/alarmz/recoll
scoop install alarmz/recoll
```

If you want inclusion in the main Scoop community buckets, submit the manifest to `ScoopInstaller/Extras`.

## winget

Submit the files in [winget/](/home/alarm/recoll/winget) to `microsoft/winget-pkgs` in a pull request.

Official submission docs:

- https://learn.microsoft.com/en-us/windows/package-manager/package/repository

## Chocolatey

The package skeleton lives in [packaging/chocolatey](/home/alarm/recoll/packaging/chocolatey).

Typical publish flow:

1. Pack on Windows with `choco pack`.
2. Test install locally.
3. Push with `choco push`.

Official docs:

- https://docs.chocolatey.org/en-us/create/create-packages/
