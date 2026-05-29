#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"
qmake6 GifSlim.pro
make -j"$(nproc)"
