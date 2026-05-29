#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

if [[ "${OS:-}" != "Windows_NT" && "$(uname -s)" != MINGW* && "$(uname -s)" != MSYS* && "$(uname -s)" != CYGWIN* ]]; then
    echo "Windows 包需要在原生 Windows / MSYS2 环境中构建（需要 qmake6、windeployqt、giflib 运行库，以及可选的 Inno Setup）。" >&2
    exit 1
fi

VERSION="${VERSION:-1.0.0}"
DIST_DIR="$(pwd)/dist"
PORTABLE_DIR="$DIST_DIR/windows-portable"

mkdir -p "$PORTABLE_DIR"

qmake6 GifEditor.pro
make -j"$(nproc 2>/dev/null || echo 4)"

cp gif-editor.exe "$PORTABLE_DIR/gif-editor.exe"
windeployqt "$PORTABLE_DIR/gif-editor.exe"

if command -v iscc >/dev/null 2>&1; then
    iscc packaging/windows/GifEditor.iss
else
    (cd "$DIST_DIR" && zip -r "GifEditor-${VERSION}-windows-x64-portable.zip" windows-portable)
fi
