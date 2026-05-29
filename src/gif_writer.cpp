#include "gif_writer.h"

#include <QFile>
#include <QDebug>
#include <gif_lib.h>
#include <cstring>
#include <cstdlib>
#include <climits>

// Map RGBA pixel to nearest color in a palette
static int nearestColor(QRgb pixel, const QVector<QRgb> &palette) {
    int bestIdx = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < palette.size(); i++) {
        int dr = qRed(pixel) - qRed(palette[i]);
        int dg = qGreen(pixel) - qGreen(palette[i]);
        int db = qBlue(pixel) - qBlue(palette[i]);
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) { bestDist = dist; bestIdx = i; }
    }
    return bestIdx;
}

// Sample frames across the animation and build a representative global palette
static QVector<QRgb> buildPalette(const std::function<QImage(int)> &frameSource,
                                   int totalFrames, int targetColors) {
    constexpr int kMaxSamplePixels = 800 * 600 * 8;  // Cap memory for sampling
    int sampleCount = std::min(totalFrames, 16);
    int step = std::max(1, totalFrames / sampleCount);
    sampleCount = (totalFrames + step - 1) / step;

    // Build a combined strip from sampled frames
    int stripW = 0;
    QVector<QImage> samples;
    for (int i = 0; i < sampleCount; i++) {
        QImage img = frameSource(i * step);
        if (img.isNull()) continue;
        QImage rgba = img.convertToFormat(QImage::Format_ARGB32);
        int sw = rgba.width(), sh = rgba.height();
        // Scale down large frames for sampling to save memory
        if (sw * sh * sampleCount > kMaxSamplePixels) {
            double ratio = std::sqrt(double(kMaxSamplePixels) / (sw * sh * sampleCount));
            sw = std::max(1, int(sw * ratio));
            sh = std::max(1, int(sh * ratio));
            rgba = rgba.scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        samples.append(rgba);
        stripW = std::max(stripW, sw);
    }

    if (samples.isEmpty()) {
        QVector<QRgb> fallback;
        fallback.reserve(256);
        for (int i = 0; i < 256; i++)
            fallback.append(qRgb(i, i, i));
        return fallback;
    }

    // Concatenate sampled frames side-by-side into one image
    int totalW = 0, maxH = 0;
    for (const auto &s : samples) {
        totalW += s.width();
        maxH = std::max(maxH, s.height());
    }

    QImage combined(totalW, maxH, QImage::Format_ARGB32);
    combined.fill(Qt::transparent);
    int xOff = 0;
    for (const auto &s : samples) {
        for (int y = 0; y < s.height(); y++) {
            const auto *src = reinterpret_cast<const QRgb *>(s.constScanLine(y));
            auto *dst = reinterpret_cast<QRgb *>(combined.scanLine(y)) + xOff;
            std::memcpy(dst, src, s.width() * 4);
        }
        xOff += s.width();
    }

    QImage indexed = combined.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    QVector<QRgb> palette = indexed.colorTable();
    if (targetColors < 256 && palette.size() > targetColors)
        palette.resize(targetColors);
    return palette;
}

bool GifWriter::write(const QString &path,
                      const std::function<QImage(int)> &frameSource,
                      const std::vector<int> &durations,
                      int loop,
                      int colorCount,
                      ProgressFn onProgress)
{
    int total = static_cast<int>(durations.size());
    if (total == 0) return false;

    QImage first = frameSource(0);
    if (first.isNull()) return false;
    int w = first.width(), h = first.height();
    if (w <= 0 || h <= 0) return false;

    // Build global palette from sampled frames (not just the first)
    QVector<QRgb> globalPalette = buildPalette(frameSource, total, colorCount);

    // Open GIF file
    QByteArray pathBytes = path.toUtf8();
    int err = 0;
    GifFileType *gif = EGifOpenFileName(pathBytes.constData(), false, &err);
    if (!gif) { qWarning() << "EGifOpenFileName failed:" << err; return false; }

    EGifSetGifVersion(gif, true);

    gif->SWidth = w; gif->SHeight = h;
    gif->SColorResolution = 8;
    gif->SBackGroundColor = 0;

    ColorMapObject *cmap = GifMakeMapObject(256, NULL);
    if (!cmap) { EGifCloseFile(gif, &err); return false; }
    int nColors = globalPalette.size();
    for (int i = 0; i < 256; i++) {
        if (i < nColors) {
            QRgb c = globalPalette[i];
            cmap->Colors[i].Red   = qRed(c);
            cmap->Colors[i].Green = qGreen(c);
            cmap->Colors[i].Blue  = qBlue(c);
        } else {
            cmap->Colors[i].Red = cmap->Colors[i].Green = cmap->Colors[i].Blue = 0;
        }
    }
    gif->SColorMap = cmap;
    EGifPutScreenDesc(gif, w, h, 8, 0, cmap);

    // NETSCAPE loop extension
    if (loop >= 0) {
        EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE);
        char nsle[11] = {'N','E','T','S','C','A','P','E','2','.','0'};
        EGifPutExtensionBlock(gif, 11, nsle);
        unsigned char sub[3] = {1, (unsigned char)(loop & 0xFF), (unsigned char)((loop >> 8) & 0xFF)};
        EGifPutExtensionBlock(gif, 3, sub);
        EGifPutExtensionTrailer(gif);
    }

    for (int i = 0; i < total; i++) {
        QImage src = frameSource(i);
        if (src.isNull()) continue;
        QImage rgba = src.convertToFormat(QImage::Format_ARGB32);

        QImage indexed(w, h, QImage::Format_Indexed8);
        indexed.setColorTable(globalPalette);
        for (int y = 0; y < h; y++) {
            auto *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
            for (int x = 0; x < w; x++)
                indexed.setPixel(x, y, nearestColor(line[x], globalPalette));
        }

        // Graphic Control Extension
        int delay = (i < (int)durations.size()) ? durations[i] : 100;
        int cs = delay / 10;  if (cs < 1) cs = 1;
        GifByteType gceBuf[8];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = 2;
        gcb.UserInputFlag = false;
        gcb.DelayTime = cs;
        gcb.TransparentColor = -1;
        size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);

        EGifPutImageDesc(gif, 0, 0, w, h, 0, NULL);

        std::vector<GifByteType> row(w);
        for (int y = 0; y < h; y++) {
            const uchar *scan = indexed.constScanLine(y);
            std::memcpy(row.data(), scan, w);
            EGifPutLine(gif, row.data(), w);
        }

        if (onProgress) onProgress(i + 1, total);
    }

    EGifCloseFile(gif, &err);
    GifFreeMapObject(cmap);

    if (err != 0) { qWarning() << "EGifCloseFile err:" << err; return false; }
    return true;
}

qint64 GifWriter::estimateSize(int frameCount, int width, int height,
                                int colors, double scale, int keepEvery,
                                double speedFactor)
{
    Q_UNUSED(speedFactor);
    int n = std::max(1, frameCount / keepEvery);
    int w = std::max(1, static_cast<int>(width * scale));
    int h = std::max(1, static_cast<int>(height * scale));
    qint64 raw = static_cast<qint64>(w) * h;  // 1 byte/pixel (indexed)
    // GIF LZW typical ratios for animated content, conservative side
    int div = 5;          // 256 colors → ~5:1
    if (colors <= 128) div = 8;
    if (colors <= 64)  div = 14;
    if (colors <= 32)  div = 22;
    if (colors <= 16)  div = 32;
    return raw / div * n + 2048;  // + header/palette/loop extension overhead
}
