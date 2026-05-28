#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
qmake6 GifEditor.pro
make -j"$(nproc)"
