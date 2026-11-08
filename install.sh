#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if [ "$(id -u)" -eq 0 ]; then
  echo "Run this installer as your normal user, not root." >&2
  exit 1
fi
if ! command -v pacman >/dev/null 2>&1; then
  echo "This installer supports Arch Linux and Omarchy." >&2
  exit 1
fi
sudo pacman -S --needed base-devel
exec "$ROOT/desktop/bin/install" "$@"
