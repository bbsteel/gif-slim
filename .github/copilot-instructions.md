# Copilot Instructions

## Build and validation

- Primary build path: qmake, not CMake. Use `./build.sh` for the standard local build; it regenerates the root `Makefile` with `qmake6 GifSlim.pro` and then runs `make -j$(nproc)`.
- Use `./build_and_run.sh` to rebuild and launch the desktop app through the existing `run.sh` wrapper.
- Packaging entrypoints are `./package-linux.sh`, `./package-windows.sh`, and `./package-macos.sh`. Only the Linux script is expected to run from this repository's current Linux environment; the other two are native-platform packaging scripts for Windows/macOS environments.
- The checked-in root `Makefile` is generated from `GifSlim.pro` and is currently the most reliable build entrypoint.
- No automated test suite or lint target is defined in this repository.
- Use the headless CLI path for smoke checks after code changes:

```bash
QT_QPA_PLATFORM=offscreen ./gif-slim --headless --open test.gif --skip 2 --save /tmp/out.gif
```

## High-level architecture

- `src/main.cpp` is the single entrypoint for both GUI and CLI flows. It installs file logging under `QStandardPaths::AppLocalDataLocation`, parses CLI actions, always creates a `QApplication`, then drives the same `MainWindow` editing pipeline for interactive and headless usage.
- `MainWindow` is the orchestration layer. It owns the loaded `GifReader`, the current editable frame selection in `m_activeFrames`, undo state in `m_prevActiveFrames`, and the save-time transform settings (`m_speedFactor`, `m_scaleFactor`, `m_colorCount`).
- Editing is index-based, not pixel-buffer-based. Crop/delete/skip operations rebuild `m_activeFrames`; they do not mutate GIF data immediately.
- `PlayerWidget` only handles display and playback. It uses a display-index-to-source-index mapper supplied by `MainWindow`, and `MainWindow` also sets the current playback range there, so playback, replay, and range preview all stay aligned with the current active-frame mapping.
- `EditorPanel` is now an info/actions sidebar, not a persistent parameter editor. It shows file info, readonly range labels, estimated export size, and launches per-action dialogs that emit the actual changes back to `MainWindow`.
- `GifReader` uses giflib to slurp metadata up front, then lazily decodes frames into cached `QImage`s on demand via `getFrame()`.
- `GifWriter` re-encodes the currently active frames to disk at save time. Speed changes are applied by recomputing output durations during save; scale and color changes are applied to each emitted frame before encoding.

## Key conventions

- Treat GUI ranges as closed intervals: `[start, end]`. `RangeSlider`, playback restart, crop/delete, and the readonly range labels are all expected to agree on that inclusive interpretation.
- The slider is the authoritative source of the current GUI range. `EditorPanel` only mirrors that state for display.
- GUI `save()` is a dialog-driven export flow: it shows the current effective change summary, prefills a default output filename, and only overwrites the source file if the user keeps the original name. Overwrite saves still use a `.bak` backup/restore path.
- `saveAs()` is still the non-interactive export path used by CLI/headless flows.
- Headless mode still instantiates `QApplication` and `MainWindow`; use `QT_QPA_PLATFORM=offscreen` in non-desktop environments.
- Packaging assets live under `assets/` and `packaging/`. The Linux AppImage flow depends on `packaging/linux/gif-slim.desktop` and the SVG icon in `assets/gif-slim.svg`.
- UI text and many inline comments are in Chinese. Keep new user-facing strings and nearby maintenance notes consistent with that style.
- The estimated export size shown in the sidebar is intentionally a fast approximation derived from the current active frame count and export parameters; it is not produced by a background re-encode.
- `optimizer.*` still exists as a separate helper pipeline, but the main GUI export flow now applies skip/range changes through `m_activeFrames` and applies speed/scale/color directly during export.
