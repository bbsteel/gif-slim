#include "gif_reader.h"

#include <QFileInfo>
#include <QDebug>
#include <gif_lib.h>

#include <cstring>
#include <algorithm>

GifReader::GifReader(const QString &path)
    : m_pathBytes(path.toUtf8())
{
    m_info.filePath = path;
    m_info.fileSize = QFileInfo(path).size();

    int error = 0;
    auto *gif = DGifOpenFileName(m_pathBytes.constData(), &error);
    if (!gif) {
        qWarning() << "DGifOpenFileName failed, error:" << error;
        return;
    }

    if (DGifSlurp(gif) != GIF_OK) {
        qWarning() << "DGifSlurp failed";
        DGifCloseFile(gif, nullptr);
        return;
    }

    m_info.width = gif->SWidth;
    m_info.height = gif->SHeight;
    m_info.frameCount = gif->ImageCount;

    // Extract per-frame metadata
    for (int i = 0; i < gif->ImageCount; i++) {
        const auto &img = gif->SavedImages[i];
        int delay = 100, disposal = 0, transparent = -1;
        for (int j = 0; j < img.ExtensionBlockCount; j++) {
            const auto &ext = img.ExtensionBlocks[j];
            if (ext.Function == GRAPHICS_EXT_FUNC_CODE && ext.ByteCount >= 4) {
                disposal = (ext.Bytes[0] >> 2) & 0x07;
                transparent = (ext.Bytes[0] & 1) ? ext.Bytes[3] : -1;
                delay = (ext.Bytes[2] << 8) | ext.Bytes[1];
                delay = delay * 10;
                if (delay < 10) delay = 100;
                break;
            }
        }
        m_info.durations.push_back(delay);
        m_info.totalDurationMs += delay;

        GifFrameInfo fi;
        fi.index = i;
        fi.delayMs = delay;
        fi.disposal = disposal;
        fi.transparent = transparent;
        m_info.frames.push_back(fi);
    }

    m_gifFile = gif;
    m_valid = true;

    // Pre-composite all frames
    compositeAllFrames();
}

GifReader::~GifReader() {
    if (m_gifFile) {
        DGifCloseFile(static_cast<GifFileType *>(m_gifFile), nullptr);
    }
}

void GifReader::compositeAllFrames() {
    auto *gif = static_cast<GifFileType *>(m_gifFile);
    int w = m_info.width, h = m_info.height;
    if (w <= 0 || h <= 0) return;

    // Background color
    QRgb bgColor = qRgba(0, 0, 0, 0);
    if (gif->SColorMap && gif->SBackGroundColor < gif->SColorMap->ColorCount) {
        auto &c = gif->SColorMap->Colors[gif->SBackGroundColor];
        bgColor = qRgba(c.Red, c.Green, c.Blue, 255);
    }

    // Working canvas: the visual state after drawing each frame
    QImage canvas(w, h, QImage::Format_ARGB32);
    canvas.fill(bgColor);

    // For DISPOSE_PREVIOUS (3): saved canvas state before a frame is drawn
    QImage prevCanvas;

    m_cache.resize(gif->ImageCount);

    for (int i = 0; i < gif->ImageCount; i++) {
        const auto &saved = gif->SavedImages[i];
        const auto &fi = m_info.frames[i];

        int fw = (int)saved.ImageDesc.Width;
        int fh = (int)saved.ImageDesc.Height;
        int left = (int)saved.ImageDesc.Left;
        int top = (int)saved.ImageDesc.Top;

        // Apply disposal from PREVIOUS frame
        if (i > 0) {
            const auto &prev = gif->SavedImages[i - 1];
            const auto &prevFi = m_info.frames[i - 1];
            int pw = (int)prev.ImageDesc.Width;
            int ph = (int)prev.ImageDesc.Height;
            int pLeft = (int)prev.ImageDesc.Left;
            int pTop = (int)prev.ImageDesc.Top;

            if (prevFi.disposal == 2) {
                // Restore to background
                for (int y = 0; y < ph; y++) {
                    int cy = pTop + y;
                    if (cy < 0 || cy >= h) continue;
                    auto *line = reinterpret_cast<QRgb *>(canvas.scanLine(cy)) + pLeft;
                    for (int x = 0; x < pw; x++) {
                        int cx = pLeft + x;
                        if (cx < 0 || cx >= w) continue;
                        line[x] = bgColor;
                    }
                }
            } else if (prevFi.disposal == 3 && !prevCanvas.isNull()) {
                // Restore to previous canvas state (only the affected rect)
                for (int y = 0; y < ph; y++) {
                    int cy = pTop + y;
                    if (cy < 0 || cy >= h) continue;
                    auto *dst = reinterpret_cast<QRgb *>(canvas.scanLine(cy)) + pLeft;
                    auto *src = reinterpret_cast<const QRgb *>(prevCanvas.constScanLine(cy)) + pLeft;
                    for (int x = 0; x < pw; x++) {
                        int cx = pLeft + x;
                        if (cx < 0 || cx >= w) continue;
                        dst[x] = src[x];
                    }
                }
            }
            // disposal 0/1: leave in place (do nothing)
        }

        // Save pre-draw canvas if NEXT frame uses DISPOSE_PREVIOUS
        if (i + 1 < gif->ImageCount && m_info.frames[i + 1].disposal == 3) {
            prevCanvas = canvas.copy();
        }

        // Draw current frame
        ColorMapObject *cmap = saved.ImageDesc.ColorMap
            ? saved.ImageDesc.ColorMap : gif->SColorMap;
        if (cmap) {
            const auto *raster = saved.RasterBits;
            for (int y = 0; y < fh; y++) {
                int cy = top + y;
                if (cy < 0 || cy >= h) continue;
                auto *line = reinterpret_cast<QRgb *>(canvas.scanLine(cy)) + left;
                for (int x = 0; x < fw; x++) {
                    int cx = left + x;
                    if (cx < 0 || cx >= w) continue;
                    int idx = raster[y * fw + x];
                    if (idx == fi.transparent) continue;
                    if (idx >= 0 && idx < cmap->ColorCount) {
                        auto &entry = cmap->Colors[idx];
                        line[x] = qRgba(entry.Red, entry.Green, entry.Blue, 255);
                    }
                }
            }
        }

        // Copy composited result to cache
        m_cache[i] = canvas.copy();
    }
}

QImage GifReader::decodeFrame(int index) {
    // After compositeAllFrames, all frames are pre-computed in m_cache.
    // This method is kept for API compatibility; it returns the cached frame.
    if (index < 0 || index >= m_info.frameCount) return {};
    if (!m_cache[index].isNull()) return m_cache[index];

    auto *gif = static_cast<GifFileType *>(m_gifFile);
    const auto &saved = gif->SavedImages[index];

    int w = saved.ImageDesc.Width;
    int h = saved.ImageDesc.Height;
    int left = saved.ImageDesc.Left;
    int top = saved.ImageDesc.Top;

    ColorMapObject *cmap = saved.ImageDesc.ColorMap ? saved.ImageDesc.ColorMap : gif->SColorMap;
    if (!cmap) return {};

    QImage img(m_info.width, m_info.height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    const auto *raster = saved.RasterBits;
    for (int y = 0; y < h; y++) {
        auto *scanLine = reinterpret_cast<QRgb *>(img.scanLine(top + y)) + left;
        for (int x = 0; x < w; x++) {
            int idx = raster[y * w + x];
            if (idx >= 0 && idx < cmap->ColorCount) {
                auto &entry = cmap->Colors[idx];
                scanLine[x] = qRgba(entry.Red, entry.Green, entry.Blue, 255);
            }
        }
    }

    return img;
}

QImage GifReader::getFrame(int index) {
    if (index < 0 || index >= m_info.frameCount) return {};

    if (!m_cache[index].isNull())
        return m_cache[index];

    QImage img = decodeFrame(index);
    m_cache[index] = img;
    return img;
}

void GifReader::prefetch(int startIndex, int count) {
    int end = std::min(startIndex + count, m_info.frameCount);
    for (int i = startIndex; i < end; i++) {
        if (m_cache[i].isNull())
            m_cache[i] = decodeFrame(i);
    }
}

void GifReader::clearCache() {
    for (auto &img : m_cache) img = QImage();
}
