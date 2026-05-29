#pragma once

#include <QString>
#include <QImage>
#include <vector>
#include <functional>

/// 从帧序列 + durations 写入 GIF 文件
class GifWriter {
public:
    /// progressFn(completed, total) — 进度回调（在 worker 线程调用）
    using ProgressFn = std::function<void(int, int)>;

    /// 写入 GIF
    /// @param path      输出路径
    /// @param frames    帧生成器：call getFrame(i) 获取每帧
    /// @param durations 每帧 delay (ms)
    /// @param loop      0=无限循环
    /// @param colorCount 调色板颜色数 (≤256)
    /// @param onProgress 进度回调
    static bool write(const QString &path,
                      const std::function<QImage(int)> &frameSource,
                      const std::vector<int> &durations,
                      int loop = 0,
                      int colorCount = 256,
                      ProgressFn onProgress = {});

    /// 预估输出大小 (bytes)
    static qint64 estimateSize(int frameCount, int width, int height,
                               int colors, double scale, int keepEvery,
                               double speedFactor);
};
