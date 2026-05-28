#include "gif_reader.h"

#include <QFileInfo>
#include <QDebug>
#include <gif_lib.h>

#include <cstring>
#include <algorithm>

// ── helpers ──

static int readFunc(GifFileType *gif, GifByteType *buf, int len) {
    auto *file = static_cast<QFile *>(gif->UserData);
    return static_cast<int>(file->read(reinterpret_cast<char *>(buf), len));
}

GifReader::GifReader(const QString &path)
    : m_pathBytes(path.toUtf8())
{
    m_info.filePath = path;
    m_info.fileSize = QFileInfo(path).size();

    // 用 QFile 打开
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open:" << path;
        return;
    }

    // giflib 需要 FILE* 或用自定义 IO —— 用 QFile 适配
    // 简便方法：直接用 DGifOpenFileName
    int error = 0;
    auto *gif = DGifOpenFileName(m_pathBytes.constData(), &error);
    if (!gif) {
        qWarning() << "DGifOpenFileName failed, error:" << error;
        return;
    }

    // DGifSlurp 一次性读取全部帧（giflib 内部格式，不转 QImage）
    if (DGifSlurp(gif) != GIF_OK) {
        qWarning() << "DGifSlurp failed";
        DGifCloseFile(gif, nullptr);
        return;
    }

    m_info.width = gif->SWidth;
    m_info.height = gif->SHeight;
    m_info.frameCount = gif->ImageCount;

    // 收集每帧信息
    for (int i = 0; i < gif->ImageCount; i++) {
        const auto &img = gif->SavedImages[i];
        int delay = 100; // default 100ms
        for (int j = 0; j < img.ExtensionBlockCount; j++) {
            const auto &ext = img.ExtensionBlocks[j];
            if (ext.Function == GRAPHICS_EXT_FUNC_CODE && ext.ByteCount >= 4) {
                delay = (ext.Bytes[2] << 8) | ext.Bytes[1];
                delay = delay * 10; // 百分秒 → 毫秒
                if (delay < 10) delay = 100;
                break;
            }
        }
        m_info.durations.push_back(delay);
        m_info.totalDurationMs += delay;

        GifFrameInfo fi;
        fi.index = i;
        fi.delayMs = delay;
        m_info.frames.push_back(fi);
    }

    m_cache.resize(gif->ImageCount);
    for (auto &img : m_cache) img = QImage();

    m_gifFile = gif;
    m_valid = true;
}

GifReader::~GifReader() {
    if (m_gifFile) {
        DGifCloseFile(static_cast<GifFileType *>(m_gifFile), nullptr);
    }
}

QImage GifReader::decodeFrame(int index) {
    if (index < 0 || index >= m_info.frameCount) return {};

    auto *gif = static_cast<GifFileType *>(m_gifFile);
    const auto &saved = gif->SavedImages[index];

    int w = saved.ImageDesc.Width;
    int h = saved.ImageDesc.Height;
    int left = saved.ImageDesc.Left;
    int top = saved.ImageDesc.Top;

    // 获取调色板
    ColorMapObject *cmap = saved.ImageDesc.ColorMap ? saved.ImageDesc.ColorMap : gif->SColorMap;
    if (!cmap) return {};

    // 构建 RGBA 图像 (全尺寸)
    QImage img(m_info.width, m_info.height, QImage::Format_RGBA8888);
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

    // 缓存命中
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
