#include "gif_writer.h"

#include <QFile>
#include <QDebug>
#include <gif_lib.h>
#include <cstring>
#include <cstdlib>
#include <climits>

// 6-bits-per-channel 3D lookup table for O(1) nearest-color mapping
// Instead of scanning the full palette per pixel (O(w*h*|P|)), we precompute
// the nearest palette index for each quantized RGB bucket and do one lookup.
static void buildColorLUT(unsigned char lut[64][64][64], const QVector<QRgb> &palette) {
    int n = palette.size();
    for (int r = 0; r < 64; r++) {
        for (int g = 0; g < 64; g++) {
            for (int b = 0; b < 64; b++) {
                int bestIdx = 0;
                int bestDist = INT_MAX;
                int pr = r * 4 + 2;   // decode bucket center (approx)
                int pg = g * 4 + 2;
                int pb = b * 4 + 2;
                for (int i = 0; i < n; i++) {
                    int dr = pr - qRed(palette[i]);
                    int dg = pg - qGreen(palette[i]);
                    int db = pb - qBlue(palette[i]);
                    int dist = dr * dr + dg * dg + db * db;
                    if (dist < bestDist) { bestDist = dist; bestIdx = i; }
                }
                lut[r][g][b] = (unsigned char)bestIdx;
            }
        }
    }
}

static inline int nearestColor(QRgb pixel, const unsigned char lut[64][64][64]) {
    return lut[(qRed(pixel) >> 2) & 63][(qGreen(pixel) >> 2) & 63][(qBlue(pixel) >> 2) & 63];
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

    // Build 3D LUT for O(1) nearest-color mapping
    unsigned char (*lut)[64][64] = new unsigned char[64][64][64];
    buildColorLUT(lut, globalPalette);

    // Quantize all frames to indexed first, then diff consecutive frames
    QVector<QImage> indexedFrames(total);
    for (int i = 0; i < total; i++) {
        QImage src = frameSource(i);
        if (src.isNull()) continue;
        QImage rgba = src.convertToFormat(QImage::Format_ARGB32);

        QImage indexed(w, h, QImage::Format_Indexed8);
        indexed.setColorTable(globalPalette);
        // Direct scanline access avoids setPixel() overhead per pixel
        for (int y = 0; y < h; y++) {
            auto *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
            uchar *dst = indexed.scanLine(y);
            for (int x = 0; x < w; x++) {
                QRgb p = line[x];
                dst[x] = lut[(qRed(p) >> 2) & 63][(qGreen(p) >> 2) & 63][(qBlue(p) >> 2) & 63];
            }
        }
        indexedFrames[i] = indexed;
        if (onProgress && (i % 10) == 0) onProgress(i + 1, total);
    }
    delete[] lut;

    // Write frame 0 as full canvas
    {
        const QImage &img = indexedFrames[0];
        int delay = (0 < (int)durations.size()) ? durations[0] : 100;
        int cs = delay / 10;  if (cs < 1) cs = 1;
        GifByteType gceBuf[8];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = 0;  // leave in place (frames diff against this)
        gcb.UserInputFlag = false;
        gcb.DelayTime = cs;
        gcb.TransparentColor = -1;
        size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);
        EGifPutImageDesc(gif, 0, 0, w, h, 0, NULL);
        std::vector<GifByteType> row(w);
        for (int y = 0; y < h; y++) {
            std::memcpy(row.data(), img.constScanLine(y), w);
            EGifPutLine(gif, row.data(), w);
        }
        if (onProgress) onProgress(1, total);
    }

    // Write subsequent frames as diff rectangles against previous frame
    for (int i = 1; i < total; i++) {
        const QImage &cur = indexedFrames[i];
        const QImage &prev = indexedFrames[i - 1];
        if (cur.isNull() || prev.isNull()) continue;

        // Find bounding box of changed pixels
        int minX = w, minY = h, maxX = -1, maxY = -1;
        for (int y = 0; y < h; y++) {
            const uchar *pRow = prev.constScanLine(y);
            const uchar *cRow = cur.constScanLine(y);
            for (int x = 0; x < w; x++) {
                if (pRow[x] != cRow[x]) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        if (minX > maxX) {
            // No change at all — write a 1×1 transparent placeholder
            minX = 0; minY = 0; maxX = 0; maxY = 0;
        }

        int bw = maxX - minX + 1;
        int bh = maxY - minY + 1;

        int delay = (i < (int)durations.size()) ? durations[i] : 100;
        int cs = delay / 10;  if (cs < 1) cs = 1;
        GifByteType gceBuf[8];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = 0;  // leave in place
        gcb.UserInputFlag = false;
        gcb.DelayTime = cs;
        gcb.TransparentColor = -1;
        size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);

        EGifPutImageDesc(gif, minX, minY, bw, bh, 0, NULL);

        std::vector<GifByteType> row(bw);
        for (int y = 0; y < bh; y++) {
            std::memcpy(row.data(), cur.constScanLine(minY + y) + minX, bw);
            EGifPutLine(gif, row.data(), bw);
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
    // Frame 0 is full-canvas; subsequent frames are diff rectangles.
    // Conservatively estimate ~30% change per frame on average.
    qint64 fullFirst = raw / 5;           // first frame: full canvas, ~5:1 LZW
    qint64 perDiff   = raw * 3 / 10 / 5;  // diff frames: ~30% area, ~5:1 LZW
    if (colors <= 128) { fullFirst = raw / 8;  perDiff = raw * 3 / 10 / 8; }
    if (colors <= 64)  { fullFirst = raw / 14; perDiff = raw * 3 / 10 / 14; }
    if (colors <= 32)  { fullFirst = raw / 22; perDiff = raw * 3 / 10 / 22; }
    if (colors <= 16)  { fullFirst = raw / 32; perDiff = raw * 3 / 10 / 32; }
    return fullFirst + perDiff * std::max(0, n - 1) + 2048;
}
