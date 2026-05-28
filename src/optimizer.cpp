#include "optimizer.h"
#include "gif_reader.h"

#include <algorithm>

OptimizeResult optimize(GifReader &reader,
                        const OptimizeParams &params,
                        std::function<QImage(int)> &outFrameSource)
{
    const auto &info = reader.info();
    int n = info.frameCount;
    int end = (params.endFrame < 0) ? n : std::min(params.endFrame, n);
    int start = std::max(0, params.startFrame);

    OptimizeResult result;
    result.outputWidth  = std::max(1, static_cast<int>(info.width * params.scale));
    result.outputHeight = std::max(1, static_cast<int>(info.height * params.scale));

    // 收集输出 durations（用于 writer）
    for (int i = start; i < end; i += params.keepEvery) {
        int dur = (i < static_cast<int>(info.durations.size()))
            ? info.durations[i] : 100;
        dur = std::max(10, static_cast<int>(dur / params.speedFactor));
        result.durations.push_back(dur);
    }
    result.outputFrameCount = static_cast<int>(result.durations.size());

    // 帧生成闭包：按参数过滤/变换
    // 需要保持 reader 引用 + params 值的副本
    auto keepEvery = params.keepEvery;
    auto scale = params.scale;
    auto colors = params.colors;
    auto outputStart = start;

    outFrameSource = [&reader, keepEvery, scale, colors, outputStart](int index) -> QImage {
        int srcIndex = outputStart + index * keepEvery;
        QImage img = reader.getFrame(srcIndex);
        if (img.isNull()) return {};

        // 缩放
        if (scale != 1.0) {
            int w = std::max(1, static_cast<int>(img.width() * scale));
            int h = std::max(1, static_cast<int>(img.height() * scale));
            img = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        // 降色
        if (colors < 256) {
            img = img.convertToFormat(QImage::Format_Indexed8,
                                      Qt::DiffuseDither);
            // Qt 自动使用最优 256 色调色板——我们限制到更少
            auto ct = img.colorTable();
            if (ct.size() > colors) {
                // 重新量化为 target colors
                QImage reduced(img.size(), QImage::Format_Indexed8);
                reduced.setColorCount(colors);
                // 用 k-means 风格：直接用 scaled 再量化更少色
                img = img.scaled(img.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                img = img.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
                // 强制降色到指定数量
                img.setColorCount(colors);
            }
        }

        return img;
    };

    return result;
}
