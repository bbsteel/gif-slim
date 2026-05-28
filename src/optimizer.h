#pragma once

#include <QImage>
#include <vector>
#include <functional>

class GifReader;

struct OptimizeParams {
    int keepEvery = 1;         // 每 N 帧保留 1 帧
    int startFrame = 0;
    int endFrame = -1;         // -1 = 全部
    double scale = 1.0;        // 缩放比例
    double speedFactor = 1.0;  // 播放速度倍率
    int colors = 256;          // 调色板颜色数
};

struct OptimizeResult {
    std::vector<int> durations;
    int outputFrameCount = 0;
    int outputWidth = 0;
    int outputHeight = 0;
};

/// 瘦身引擎。输入 reader + 参数，输出帧生成函数 + 元数据。
OptimizeResult optimize(GifReader &reader,
                        const OptimizeParams &params,
                        std::function<QImage(int)> &outFrameSource);
