#pragma once

#include <QImage>
#include <QTimer>
#include <QWidget>

#include <functional>

class GifReader;

class PlayerWidget : public QWidget {
    Q_OBJECT

public:
    explicit PlayerWidget(QWidget *parent = nullptr);

    void setReader(GifReader *reader);
    void setFrameCount(int n) { m_frameCount = n; }
    void setPlayRange(int start, int stop);
    void setSrcMapper(std::function<int(int)> fn) { m_srcMapper = fn; }
    int currentFrame() const { return m_frameIndex; }
    int totalFrames() const { return m_frameCount; }
    bool isPlaying() const { return m_playing; }

public slots:
    void showFrame(int index);
    void play();
    void pause();
    void togglePlay();
    void prevFrame();
    void nextFrame();

signals:
    void frameChanged(int index);
    void playbackEnded();
    void playingChanged(bool playing);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;

private:
    void setPlaying(bool playing);
    int frameDurationForDisplayIndex(int index) const;
    int lastPlayableFrame() const;

    GifReader *m_reader = nullptr;
    int m_frameIndex = 0;
    int m_frameCount = 0;
    int m_rangeStart = 0;
    int m_stopFrame = 0;
    std::function<int(int)> m_srcMapper;
    QImage m_currentImage;
    QTimer m_timer;
    bool m_playing = false;
};
