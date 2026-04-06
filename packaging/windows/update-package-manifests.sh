#!/usr/bin/env bash
set -euo pipefail

repo="${1:-alarmz/recoll}"
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
release_json="$(curl -fsSL "https://api.github.com/repos/${repo}/releases/latest")"

win_asset_json="$(printf '%s\n' "$release_json" | jq -r '
  .assets[]
  | select(.name | test("win64-setup\\.exe$"))
')"

if [[ -z "$win_asset_json" ]]; then
  echo "No Windows setup asset found in latest release for ${repo}" >&2
  exit 1
fi

version="$(
  awk -F'"' '/^#define MyAppVersion / { print $2; exit }' "$root_dir/src/windows/recoll-setup.iss" \
    | tr -d '\r' \
    | cut -d- -f1
)"
url="$(printf '%s\n' "$win_asset_json" | jq -r '.browser_download_url')"
hash="$(printf '%s\n' "$win_asset_json" | jq -r '.digest | sub("^sha256:"; "")')"
release_url="$(printf '%s\n' "$release_json" | jq -r '.html_url')"
url_with_fragment="${url}#/setup.exe"
escaped_url_with_fragment="$(printf '%s\n' "$url_with_fragment" | sed 's/[&|]/\\&/g')"
escaped_hash="$(printf '%s\n' "$hash" | sed 's/[&|]/\\&/g')"
escaped_url="$(printf '%s\n' "$url" | sed 's/[&|]/\\&/g')"
escaped_release_url="$(printf '%s\n' "$release_url" | sed 's/[&|]/\\&/g')"

sed -i \
  -e "s|\"url\": \".*\"|\"url\": \"${escaped_url_with_fragment}\"|" \
  -e "s|\"hash\": \".*\"|\"hash\": \"${escaped_hash}\"|" \
  -e "s|\"version\": \".*\"|\"version\": \"${version}\"|" \
  "$root_dir/recoll.json" \
  "$root_dir/bucket/recoll.json"

sed -i \
  -e "s#^PackageVersion: .*#PackageVersion: ${version}#" \
  "$root_dir/winget/alarmz.Recoll.yaml" \
  "$root_dir/winget/alarmz.Recoll.installer.yaml" \
  "$root_dir/winget/alarmz.Recoll.locale.en-US.yaml"

sed -i \
  -e "s|^    InstallerUrl: .*|    InstallerUrl: ${escaped_url}|" \
  -e "s|^    InstallerSha256: .*|    InstallerSha256: ${hash^^}|" \
  "$root_dir/winget/alarmz.Recoll.installer.yaml"

sed -i \
  -e "s|^ReleaseNotesUrl: .*|ReleaseNotesUrl: ${escaped_release_url}|" \
  "$root_dir/winget/alarmz.Recoll.locale.en-US.yaml"

sed -i \
  -e "s|<version>.*</version>|<version>${version}</version>|" \
  -e "s|<releaseNotes>.*</releaseNotes>|<releaseNotes>${escaped_release_url}</releaseNotes>|" \
  "$root_dir/packaging/chocolatey/recoll.install.nuspec"

sed -i \
  -e "s|\\\$url64 = '.*'|\\\$url64 = '${escaped_url}'|" \
  -e "s|\\\$checksum64 = '.*'|\\\$checksum64 = '${hash}'|" \
  "$root_dir/packaging/chocolatey/tools/chocolateyinstall.ps1"

sed -i \
  -e "s|^   https://github.com/.*/releases/download/.*|   ${escaped_url}|" \
  -e "s|^   [0-9a-f][0-9a-f]*|   ${hash}|" \
  -e "s|^   https://github.com/.*/releases/tag/.*|   ${escaped_release_url}|" \
  "$root_dir/packaging/chocolatey/tools/VERIFICATION.txt"

printf 'Updated manifests for version %s\n' "$version"
printf 'URL: %s\n' "$url"
printf 'SHA256: %s\n' "$hash"
