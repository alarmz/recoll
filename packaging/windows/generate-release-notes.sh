#!/usr/bin/env bash
set -euo pipefail

repo="${1:-alarmz/recoll}"
root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

version="${VERSION:-$(
  awk -F'"' '/^#define MyAppVersion / { print $2; exit }' "$root_dir/src/windows/recoll-setup.iss" \
    | tr -d '\r' \
    | cut -d- -f1
)}"
release_tag="${RELEASE_TAG:-v${version}}"
release_url="${RELEASE_URL:-}"
win_asset_name="${WIN_ASSET_NAME:-}"
win_asset_url="${WIN_ASSET_URL:-}"
win_sha256="${WIN_SHA256:-}"

if [[ -z "$release_url" || -z "$win_asset_name" || -z "$win_asset_url" || -z "$win_sha256" ]]; then
  if ! release_json="$(curl -fsSL "https://api.github.com/repos/${repo}/releases/tags/${release_tag}")"; then
    cat >&2 <<EOF
Could not fetch GitHub release ${release_tag} for ${repo}.

Either create that GitHub release first, or provide the values explicitly:

  VERSION=${version} \\
  RELEASE_TAG=${release_tag} \\
  RELEASE_URL=https://github.com/${repo}/releases/tag/${release_tag} \\
  WIN_ASSET_NAME=recoll-win64-setup.exe \\
  WIN_ASSET_URL=https://github.com/${repo}/releases/download/${release_tag}/recoll-win64-setup.exe \\
  WIN_SHA256=<sha256> \\
  ./packaging/windows/generate-release-notes.sh
EOF
    exit 1
  fi

  win_asset_json="$(
    printf '%s\n' "$release_json" | jq -c '
      .assets[]
      | select(.name | test("win64-setup\\.exe$"))
    ' | head -n 1
  )"

  if [[ -z "$win_asset_json" ]]; then
    echo "No Windows setup asset found in release ${release_tag} for ${repo}" >&2
    exit 1
  fi

  release_url="${RELEASE_URL:-$(printf '%s\n' "$release_json" | jq -r '.html_url')}"
  win_asset_name="${WIN_ASSET_NAME:-$(printf '%s\n' "$win_asset_json" | jq -r '.name')}"
  win_asset_url="${WIN_ASSET_URL:-$(printf '%s\n' "$win_asset_json" | jq -r '.browser_download_url')}"
  win_sha256="${WIN_SHA256:-$(printf '%s\n' "$win_asset_json" | jq -r '.digest | sub("^sha256:"; "")')}"
fi

output_path="${OUTPUT_PATH:-$root_dir/releases/${release_tag}.md}"
mkdir -p "$(dirname "$output_path")"

sed \
  -e "s|{{VERSION}}|${version}|g" \
  -e "s|{{RELEASE_TAG}}|${release_tag}|g" \
  -e "s|{{RELEASE_URL}}|${release_url}|g" \
  -e "s|{{WIN_ASSET_NAME}}|${win_asset_name}|g" \
  -e "s|{{WIN_ASSET_URL}}|${win_asset_url}|g" \
  -e "s|{{WIN_SHA256}}|${win_sha256}|g" \
  "$root_dir/releases/template.md" > "$output_path"

printf 'Generated release notes: %s\n' "$output_path"
printf 'Release tag: %s\n' "$release_tag"
printf 'Installer: %s\n' "$win_asset_name"
printf 'SHA256: %s\n' "$win_sha256"
