#!/bin/bash
set -euo pipefail

# 依赖命令一览（均在 MSYS2 UCRT64 shell 中可用）：
#   qmake6      → pacman -S mingw-w64-ucrt-x86_64-qt6-base
#   make        → pacman -S make
#   nproc       → pacman -S coreutils（MSYS2 默认已装）
#   windeployqt → pacman -S mingw-w64-ucrt-x86_64-qt6-tools
#   giflib DLL  → pacman -S mingw-w64-ucrt-x86_64-giflib
#   libgcc_s_seh-1.dll, libstdc++-6.dll → mingw-w64-ucrt-x86_64-gcc 自带
#   ldd         → pacman -S mingw-w64-ucrt-x86_64-binutils（MSYS2 默认已装）
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

# Use ldd to automatically discover and copy all MinGW/GCC DLLs
# that windeployqt doesn't collect (giflib, harfbuzz, gcc runtime, etc.).
copy_missing_deps() {
    local exe_dir="$1"
    local search_dirs="/ucrt64/bin /mingw64/bin"

    # Collect all DLLs already present (lowercased for case-insensitive comparison)
    local have
    have=$(cd "$exe_dir" && ls *.dll 2>/dev/null | tr '[:upper:]' '[:lower:]')

    ldd "$exe_dir/gif-editor.exe" 2>/dev/null | while IFS= read -r line; do
        case "$line" in *" => "* ) ;; *) continue ;; esac

        local dll_path
        dll_path=$(echo "$line" | sed -n 's/.*=> *\([^ ]*\.dll\).*/\1/p')
        [ -z "$dll_path" ] && continue

        local dll_name
        dll_name=$(basename "$dll_path" | tr '[:upper:]' '[:lower:]')

        # Qt DLLs: already handled by windeployqt
        case "$dll_name" in
            qt*|Qt*|qwindows*|qoffscreen*|qminimal*) continue ;;
        esac

        # Windows system DLLs: don't bundle
        case "$dll_name" in
            kernel32*|user32*|gdi32*|comctl32*|ole32*|shell32*|advapi32*)
                continue ;;
            ntdll*|msvcrt*|ws2_32*|d3d*|opengl32*|glu32*) continue ;;
            setupapi*|winmm*|imm32*|version*|shlwapi*|usp10*|comdlg32*) continue ;;
            gdiplus*|oleaut32*|bcrypt*|crypt32*|secur32*|netapi32*) continue ;;
            mpr*|dnsapi*|iphlpapi*|winhttp*|wininet*|dwrite*|dxgi*|dcomp*) continue ;;
        esac

        # Check if already copied
        if echo "$have" | grep -qFx "$dll_name" 2>/dev/null; then
            continue
        fi

        # Search in MinGW dirs
        for dir in $search_dirs; do
            local src
            src=$(find "$dir" -maxdepth 1 -iname "$dll_name" -print -quit 2>/dev/null)
            if [ -n "$src" ] && [ -f "$src" ]; then
                echo "  → 拷贝 $(basename "$src")"
                cp "$src" "$exe_dir/"
                have="$have"$'\n'"$dll_name"
                break
            fi
        done
    done
}

copy_missing_deps "$PORTABLE_DIR"

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
