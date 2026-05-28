#pragma once

#include <QString>
#include <QImage>
#include <vector>
#include <cstdint>

/// GIF 帧元数据
struct GifFrameInfo {
    int index;
    int delayMs;       // 帧间隔 (ms)，最小 10ms
    int disposal;      // 处置方式
    uint32_t fileOffset; // 该帧数据在文件中的偏移（用于按需解码）
};

/// GIF 文件元信息（不持有帧数据）
struct GifInfo {
    QString filePath;
    int width = 0;
    int height = 0;
    int frameCount = 0;
    int totalDurationMs = 0;
    qint64 fileSize = 0;
    std::vector<int> durations;      // 每帧 delay (ms)
    std::vector<GifFrameInfo> frames;
};

/// 流式 GIF 读取器 — 按需逐帧解码，不一次加载全部
class GifReader {
public:
    explicit GifReader(const QString &path);
    ~GifReader();

    bool isValid() const { return m_valid; }
    const GifInfo &info() const { return m_info; }

    /// 获取第 index 帧（RGBA8888）。内部有缓存。
    QImage getFrame(int index);

    /// 预解码连续帧（用于播放预加载）
    void prefetch(int startIndex, int count);

    /// 清空缓存
    void clearCache();

    /// 获取文件路径（供 writer 使用）
    const char *filePath() const { return m_info.filePath.toUtf8().constData(); }

private:
    bool parseHeader();
    void collectDurations();

    GifInfo m_info;
    void *m_gifFile = nullptr;       // GifFileType*
    QByteArray m_pathBytes;          // 保持 C 字符串生命周期
    bool m_valid = false;

    // 简单帧缓存 (index → QImage)
    std::vector<QImage> m_cache;
    int m_cacheStart = -1;
    int m_cacheCount = 0;

    QImage decodeFrame(int index);
};
