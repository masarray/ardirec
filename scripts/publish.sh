#!/usr/bin/env bash
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "usage: scripts/publish.sh <git-remote-url>" >&2
  exit 2
fi
cd "$(dirname "$0")/.."
if [[ -d .git ]]; then
  echo "This folder is already a Git repository; review the existing remote first." >&2
  exit 1
fi
git init -b main
git add -- . ':!build' ':!build-*'
git commit -m "chore: bootstrap ardirec foundation"
git remote add origin "$1"
git push -u origin main
