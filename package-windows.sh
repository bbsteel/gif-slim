#!/bin/bash
set -euo pipefail

# 依赖命令一览（均在 MSYS2 UCRT64 shell 中可用）：
#   qmake6      → pacman -S mingw-w64-ucrt-x86_64-qt6-base
#   make        → pacman -S make
#   nproc       → pacman -S coreutils（MSYS2 默认已装）
#   windeployqt → pacman -S mingw-w64-ucrt-x86_64-qt6-tools
#   giflib DLL  → pacman -S mingw-w64-ucrt-x86_64-giflib
#   libgcc_s_seh-1.dll, libstdc++-6.dll → mingw-w64-ucrt-x86_64-gcc 自带
#   iscc        → choco install innosetup -y（需要管理员 PowerShell）
#   zip         → pacman -S zip（MSYS2 默认已装）
#   powershell  → Windows 自带
#   （choco 本身 → https://chocolatey.org/install）

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

cp release/gif-editor.exe "$PORTABLE_DIR/gif-editor.exe"
windeployqt "$PORTABLE_DIR/gif-editor.exe"

# Copy giflib DLL if available
for dll in /ucrt64/bin/libgif-7.dll /mingw64/bin/libgif-7.dll /ucrt64/bin/libgif.dll /mingw64/bin/libgif.dll; do
    if [ -f "$dll" ]; then
        cp "$dll" "$PORTABLE_DIR/"
        break
    fi
done

# Copy MinGW GCC runtime DLLs (windeployqt does not pick these up)
for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    for prefix in /ucrt64/bin /mingw64/bin; do
        if [ -f "$prefix/$dll" ]; then
            cp "$prefix/$dll" "$PORTABLE_DIR/"
            break
        fi
    done
done

if command -v iscc >/dev/null 2>&1; then
    iscc packaging/windows/GifEditor.iss
elif command -v zip >/dev/null 2>&1; then
    (cd "$DIST_DIR" && zip -r "GifEditor-${VERSION}-windows-x64-portable.zip" windows-portable)
elif command -v powershell >/dev/null 2>&1; then
    powershell -Command "Compress-Archive -Path '$PORTABLE_DIR' -DestinationPath '$DIST_DIR/GifEditor-${VERSION}-windows-x64-portable.zip'"
else
    echo "错误: 未找到 iscc、zip 或 powershell，无法打包。请安装 Inno Setup 或 zip。" >&2
    exit 1
fi
