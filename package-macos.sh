#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "macOS 包需要在原生 macOS 环境中构建（需要 qmake6、macdeployqt、giflib）。" >&2
    exit 1
fi

VERSION="${VERSION:-1.0.0}"
DIST_DIR="$(pwd)/dist"
APP_NAME="gif-editor.app"

mkdir -p "$DIST_DIR"

qmake6 GifEditor.pro
make -j"$(sysctl -n hw.ncpu)"
macdeployqt "$APP_NAME" -dmg

mv -f "gif-editor.dmg" "$DIST_DIR/GifEditor-${VERSION}-macos.dmg"
echo "$DIST_DIR/GifEditor-${VERSION}-macos.dmg"
