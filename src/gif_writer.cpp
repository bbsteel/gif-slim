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

    // Build global palette from first frame
    QImage firstIndexed = first.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    QVector<QRgb> globalPalette = firstIndexed.colorTable();
    // Limit palette size
    if (colorCount < 256 && globalPalette.size() > colorCount)
        globalPalette.resize(colorCount);

    // Open GIF file
    QByteArray pathBytes = path.toUtf8();
    int err = 0;
    GifFileType *gif = EGifOpenFileName(pathBytes.constData(), false, &err);
    if (!gif) { qWarning() << "EGifOpenFileName failed:" << err; return false; }

    EGifSetGifVersion(gif, true);

    gif->SWidth = w; gif->SHeight = h;
    gif->SColorResolution = 8;
    gif->SBackGroundColor = 0;

    // Set global color map
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

    QImage firstMapped(w, h, QImage::Format_Indexed8);
    firstMapped.setColorTable(globalPalette);
    for (int y = 0; y < h; y++) {
        auto *line = reinterpret_cast<const QRgb *>(first.constScanLine(y));
        for (int x = 0; x < w; x++)
            firstMapped.setPixel(x, y, nearestColor(line[x], globalPalette));
    }

    for (int i = 0; i < total; i++) {
        QImage frame = (i == 0) ? firstMapped : [&]() -> QImage {
            QImage src = frameSource(i);
            if (src.isNull()) return {};
            QImage indexed(w, h, QImage::Format_Indexed8);
            indexed.setColorTable(globalPalette);
            QImage rgba = src.convertToFormat(QImage::Format_RGBA8888);
            for (int y = 0; y < h; y++) {
                auto *line = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
                for (int x = 0; x < w; x++)
                    indexed.setPixel(x, y, nearestColor(line[x], globalPalette));
            }
            return indexed;
        }();

        if (frame.isNull()) continue;

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
            const uchar *src = frame.constScanLine(y);
            std::memcpy(row.data(), src, w);
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
    qint64 perFrame = static_cast<qint64>(w) * h;
    if (colors <= 64)  perFrame = perFrame * 3 / 4;
    if (colors <= 32)  perFrame = perFrame / 2;
    if (colors <= 16)  perFrame = perFrame / 3;
    return perFrame * n + 1024;
}
