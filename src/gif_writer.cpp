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
        unsigned char nsData[16] = {
            0x0B, 'N','E','T','S','C','A','P','E','2','.','0',
            0x03, (unsigned char)(loop & 0xFF), (unsigned char)((loop >> 8) & 0xFF), 0x00
        };
        EGifPutExtension(gif, APPLICATION_EXT_FUNC_CODE, 16, nsData);
    }

    // Build 3D LUT for O(1) nearest-color mapping
    unsigned char (*lut)[64][64] = new unsigned char[64][64][64];
    buildColorLUT(lut, globalPalette);

    // Write each frame.  Disposal mode 2 clears the previous frame's rectangle
    // to the background colour, so each frame can safely cover only its own
    // non-background bounding box without leaving artefacts.
    for (int i = 0; i < total; i++) {
        QImage src = frameSource(i);
        if (src.isNull()) continue;
        QImage rgba = src.convertToFormat(QImage::Format_ARGB32);

        // Quantise to global palette and build per-frame histogram
        std::vector<uchar> pixels(w * h);
        int hist[256] = {};
        for (int y = 0; y < h; y++) {
            auto *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
            uchar *dst = pixels.data() + y * w;
            for (int x = 0; x < w; x++) {
                QRgb p = line[x];
                uchar idx = lut[(qRed(p) >> 2) & 63][(qGreen(p) >> 2) & 63][(qBlue(p) >> 2) & 63];
                dst[x] = idx;
                hist[idx]++;
            }
        }

        // Per-frame background: most frequent palette index in this frame.
        // Adapts automatically to window/content switches in screen recordings.
        int bgIdx = 0, bgCount = 0;
        for (int j = 0; j < 256; j++) {
            if (hist[j] > bgCount) { bgCount = hist[j]; bgIdx = j; }
        }

        // Bounding box of non-background pixels
        int minX = w, minY = h, maxX = -1, maxY = -1;
        for (int y = 0; y < h; y++) {
            const uchar *row = pixels.data() + y * w;
            for (int x = 0; x < w; x++) {
                if (row[x] != bgIdx) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        if (minX > maxX) { minX = 0; minY = 0; maxX = 0; maxY = 0; }
        int bw = maxX - minX + 1;
        int bh = maxY - minY + 1;

        int delay = (i < (int)durations.size()) ? durations[i] : 100;
        int cs = delay / 10;  if (cs < 1) cs = 1;
        GifByteType gceBuf[8];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = 2;  // restore to background
        gcb.UserInputFlag = false;
        gcb.DelayTime = cs;
        gcb.TransparentColor = -1;
        size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);

        EGifPutImageDesc(gif, minX, minY, bw, bh, 0, NULL);

        std::vector<GifByteType> row(bw);
        for (int y = 0; y < bh; y++) {
            std::memcpy(row.data(), pixels.data() + (minY + y) * w + minX, bw);
            EGifPutLine(gif, row.data(), bw);
        }

        if (onProgress) onProgress(i + 1, total);
    }
    delete[] lut;

    // EGifCloseFile frees gif->SColorMap internally — do NOT GifFreeMapObject(cmap)
    // after this call, or we double-free.
    EGifCloseFile(gif, &err);

    if (err != 0) { qWarning() << "EGifCloseFile err:" << err; return false; }
    return true;
}

bool GifWriter::writeRaw(const QString &dstPath,
                          const QString &srcPath,
                          const std::vector<int> &frameIndices,
                          const std::vector<int> &delays)
{
    QByteArray srcBytes = srcPath.toUtf8();
    int srcErr = 0;
    GifFileType *src = DGifOpenFileName(srcBytes.constData(), &srcErr);
    if (!src) { qWarning() << "writeRaw: open src failed" << srcErr; return false; }
    if (DGifSlurp(src) != GIF_OK) { qWarning() << "writeRaw: slurp failed"; DGifCloseFile(src, nullptr); return false; }

    int n = (int)frameIndices.size();
    if (n == 0) { DGifCloseFile(src, nullptr); return false; }

    QByteArray dstBytes = dstPath.toUtf8();
    int dstErr = 0;
    GifFileType *dst = EGifOpenFileName(dstBytes.constData(), false, &dstErr);
    if (!dst) { qWarning() << "writeRaw: EGifOpenFileName failed" << dstErr; DGifCloseFile(src, nullptr); return false; }
    EGifSetGifVersion(dst, true);

    // Copy global color map
    ColorMapObject *gcm = nullptr;
    if (src->SColorMap) {
        int nc = src->SColorMap->ColorCount;
        gcm = GifMakeMapObject(nc, nullptr);
        if (gcm) {
            gcm->ColorCount = nc;
            gcm->BitsPerPixel = src->SColorMap->BitsPerPixel;
            for (int i = 0; i < nc; i++) gcm->Colors[i] = src->SColorMap->Colors[i];
        }
    }
    dst->SColorMap = gcm;
    EGifPutScreenDesc(dst, src->SWidth, src->SHeight,
                      src->SColorResolution, src->SBackGroundColor, gcm);

    // NETSCAPE loop extension
    {
        unsigned char nsData[16] = {
            0x0B, 'N','E','T','S','C','A','P','E','2','.','0',
            0x03, 0x01, 0x00, 0x00
        };
        EGifPutExtension(dst, APPLICATION_EXT_FUNC_CODE, 16, nsData);
    }

    // Copy selected frames
    for (int i = 0; i < n; i++) {
        int si = frameIndices[i];
        if (si < 0 || si >= src->ImageCount) continue;
        const SavedImage &simg = src->SavedImages[si];

        // Build GCE from original or default values
        int disposal = 0, transparent = -1, delay = 100;
        for (int j = 0; j < simg.ExtensionBlockCount; j++) {
            const ExtensionBlock &eb = simg.ExtensionBlocks[j];
            if (eb.Function == GRAPHICS_EXT_FUNC_CODE && eb.ByteCount >= 4) {
                disposal    = (eb.Bytes[0] >> 2) & 0x07;
                transparent = (eb.Bytes[0] & 1) ? eb.Bytes[3] : -1;
                delay       = ((eb.Bytes[2] << 8) | eb.Bytes[1]) * 10;
                if (delay < 10) delay = 100;
            } else if (eb.Function != APPLICATION_EXT_FUNC_CODE && eb.Function != 0) {
                // Preserve non-GCE, non-continuation per-frame extensions
                // (APPLICATION extensions like NETSCAPE are global, already written above;
                //  Function=0 blocks are continuation sub-blocks of multi-block extensions)
                EGifPutExtension(dst, eb.Function, eb.ByteCount, eb.Bytes);
            }
        }

        // Write GCE with (possibly modified) delay
        int newDelay = (i < (int)delays.size()) ? delays[i] : delay;
        int cs = newDelay / 10; if (cs < 1) cs = 1;
        {
            GifByteType gceBuf[8];
            GraphicsControlBlock gcb;
            gcb.DisposalMode = disposal;
            gcb.UserInputFlag = false;
            gcb.DelayTime = cs;
            gcb.TransparentColor = transparent;
            size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
            EGifPutExtension(dst, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);
        }

        const GifImageDesc &desc = simg.ImageDesc;
        EGifPutImageDesc(dst, desc.Left, desc.Top, desc.Width, desc.Height,
                         desc.Interlace, simg.ImageDesc.ColorMap);

        int fw = desc.Width, fh = desc.Height;
        const unsigned char *raster = simg.RasterBits;
        for (int y = 0; y < fh; y++) {
            EGifPutLine(dst, (GifPixelType *)(raster + y * fw), fw);
        }
    }

    EGifCloseFile(dst, &dstErr);
    DGifCloseFile(src, nullptr);
    if (dstErr != 0) { qWarning() << "writeRaw: close err:" << dstErr; return false; }
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
    // Frames are written as background-cropped bounding boxes.
    // Assume ~30% of canvas changes per frame on average.
    int div = 5;          // 256 colors → ~5:1 LZW
    if (colors <= 128) div = 8;
    if (colors <= 64)  div = 14;
    if (colors <= 32)  div = 22;
    if (colors <= 16)  div = 32;
    return raw * 3 / 10 / div * n + 2048;
}
