#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

VERSION="${VERSION:-1.0.0}"
ROOT_DIR="$(pwd)"
DIST_DIR="$ROOT_DIR/dist"
TOOLS_DIR="$ROOT_DIR/.packaging-tools"
APPDIR="$DIST_DIR/AppDir"

rm -rf "$APPDIR"
mkdir -p "$DIST_DIR" "$TOOLS_DIR" "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$APPDIR/usr/plugins/platforms"

./build.sh

rsvg-convert -w 256 -h 256 "$ROOT_DIR/assets/gif-slim.svg" -o "$APPDIR/usr/share/icons/hicolor/256x256/apps/gif-slim.png"
cp "$ROOT_DIR/gif-slim" "$APPDIR/usr/bin/gif-slim"
cp "$ROOT_DIR/packaging/linux/gif-slim.desktop" "$APPDIR/usr/share/applications/gif-slim.desktop"
cp /usr/lib/qt6/plugins/platforms/libqoffscreen.so "$APPDIR/usr/plugins/platforms/"
cp /usr/lib/qt6/plugins/platforms/libqminimal.so "$APPDIR/usr/plugins/platforms/"

download_tool() {
    local output="$1"
    local url="$2"
    if [ ! -f "$output" ]; then
        curl -L "$url" -o "$output"
        chmod +x "$output"
    fi
}

download_tool "$TOOLS_DIR/linuxdeploy-x86_64.AppImage" "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
download_tool "$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage" "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
download_tool "$TOOLS_DIR/appimagetool-x86_64.AppImage" "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"

ln -sf linuxdeploy-plugin-qt-x86_64.AppImage "$TOOLS_DIR/linuxdeploy-plugin-qt"
ln -sf appimagetool-x86_64.AppImage "$TOOLS_DIR/appimagetool"

export PATH="$TOOLS_DIR:$PATH"
export QMAKE=qmake6
export ARCH=x86_64
export APPIMAGE_EXTRACT_AND_RUN=1
export NO_STRIP=1

rm -f "$DIST_DIR"/GIF_Slim-*.AppImage "$DIST_DIR"/gif-slim*.AppImage "$DIST_DIR"/GifSlim-*.AppImage

"$TOOLS_DIR/linuxdeploy-x86_64.AppImage" \
    --appdir "$APPDIR" \
    -e "$APPDIR/usr/bin/gif-slim" \
    -d "$APPDIR/usr/share/applications/gif-slim.desktop" \
    -i "$APPDIR/usr/share/icons/hicolor/256x256/apps/gif-slim.png" \
    --plugin qt \
    --output appimage

generated="$(find "$ROOT_DIR" -maxdepth 1 -name '*.AppImage' | head -n 1)"
if [ -z "$generated" ]; then
    generated="$(find "$DIST_DIR" -maxdepth 1 -name '*.AppImage' | head -n 1)"
fi

if [ -z "$generated" ]; then
    echo "Linux package build failed: no AppImage was produced." >&2
    exit 1
fi

final_path="$DIST_DIR/GifSlim-${VERSION}-linux-x86_64.AppImage"
mv "$generated" "$final_path"
echo "$final_path"
