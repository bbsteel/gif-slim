#include "gif_writer.h"

#include <QFile>
#include <QDebug>
#include <gif_lib.h>
#include <cstring>
#include <cstdlib>

bool GifWriter::write(const QString &path,
                      const std::function<QImage(int)> &frameSource,
                      const std::vector<int> &durations,
                      int loop,
                      ProgressFn onProgress)
{
    QByteArray pathBytes = path.toUtf8();
    FILE *fp = fopen(pathBytes.constData(), "wb");
    if (!fp) { qWarning() << "fopen failed:" << path; return false; }

    int err = 0;
    GifFileType *gif = EGifOpenFileHandle(fileno(fp), &err);
    if (!gif) { qWarning() << "EGifOpenFileHandle failed:" << err; fclose(fp); return false; }

    EGifSetGifVersion(gif, true);  // GIF89a for animation support

    int total = static_cast<int>(durations.size());
    if (total == 0) { EGifCloseFile(gif, &err); fclose(fp); return false; }

    QImage first = frameSource(0);
    if (first.isNull()) { EGifCloseFile(gif, &err); fclose(fp); return false; }
    int w = first.width(), h = first.height();
    if (w <= 0 || h <= 0) { EGifCloseFile(gif, &err); fclose(fp); return false; }

    gif->SWidth = w; gif->SHeight = h;
    gif->SColorResolution = 8;
    gif->SBackGroundColor = 0;

    // 全局调色板 (256 色)
    QImage indexed = first.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    ColorMapObject *cmap = GifMakeMapObject(256, NULL);
    if (!cmap) { EGifCloseFile(gif, &err); fclose(fp); return false; }
    auto ct = indexed.colorTable();
    int n = ct.size();
    for (int i = 0; i < 256; i++) {
        if (i < n) {
            QRgb c = ct[i];
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
        QImage img = frameSource(i);
        if (img.isNull()) continue;
        int w2 = img.width(), h2 = img.height();
        if (w2 <= 0 || h2 <= 0) continue;
        img = img.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);

        // Graphic Control Extension
        int delay = (i < (int)durations.size()) ? durations[i] : 100;
        int cs = delay / 10;  if (cs < 1) cs = 1;
        GifByteType gceBuf[8];
        GraphicsControlBlock gcb;
        gcb.DisposalMode = 2;          // restore to background
        gcb.UserInputFlag = false;
        gcb.DelayTime = cs;
        gcb.TransparentColor = -1;     // none
        size_t gceLen = EGifGCBToExtension(&gcb, gceBuf);
        EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, (int)gceLen, gceBuf);

        EGifPutImageDesc(gif, 0, 0, w2, h2, 0, NULL);

        std::vector<GifByteType> row(w2);
        for (int y = 0; y < h2; y++) {
            const uchar *src = img.constScanLine(y);
            std::memcpy(row.data(), src, w2);
            EGifPutLine(gif, row.data(), w2);
        }

        if (onProgress) onProgress(i + 1, total);
    }

    EGifCloseFile(gif, &err);
    GifFreeMapObject(cmap);
    fclose(fp);

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
