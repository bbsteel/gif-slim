#include "player_widget.h"

#include "gif_reader.h"

#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

PlayerWidget::PlayerWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);

    connect(&m_timer, &QTimer::timeout, this, [this]() {
        if (!m_reader || m_frameCount <= 0) {
            setPlaying(false);
            return;
        }

        if (m_frameIndex >= lastPlayableFrame()) {
            setPlaying(false);
            emit playbackEnded();
            return;
        }

        showFrame(m_frameIndex + 1);
        if (m_frameIndex >= lastPlayableFrame()) {
            setPlaying(false);
            emit playbackEnded();
            return;
        }

        m_timer.start(frameDurationForDisplayIndex(m_frameIndex));
    });
}

void PlayerWidget::setReader(GifReader *reader) {
    m_reader = reader;
    m_timer.stop();
    setPlaying(false);
    m_frameIndex = 0;
    m_frameCount = reader ? reader->info().frameCount : 0;
    m_rangeStart = 0;
    m_stopFrame = m_frameCount > 0 ? m_frameCount - 1 : 0;
    m_srcMapper = nullptr;

    if (reader) {
        showFrame(0);
    } else {
        m_currentImage = QImage();
        update();
    }
}

void PlayerWidget::setPlayRange(int start, int stop) {
    if (m_frameCount <= 0) {
        m_rangeStart = 0;
        m_stopFrame = 0;
        return;
    }

    m_rangeStart = std::clamp(start, 0, m_frameCount - 1);
    m_stopFrame = std::clamp(stop, m_rangeStart, m_frameCount - 1);
}

void PlayerWidget::showFrame(int index) {
    if (!m_reader) return;
    if (index < 0 || index >= m_frameCount) return;

    m_frameIndex = index;
    int src = m_srcMapper ? m_srcMapper(index) : index;
    m_currentImage = m_reader->getFrame(src);
    update();
    emit frameChanged(index);
}

void PlayerWidget::play() {
    if (!m_reader || m_frameCount <= 0 || m_playing) return;

    if (m_frameIndex < m_rangeStart || m_frameIndex > lastPlayableFrame()) {
        showFrame(m_rangeStart);
    } else if (m_frameIndex >= lastPlayableFrame()) {
        showFrame(m_rangeStart);
    }

    setPlaying(true);
    m_timer.start(frameDurationForDisplayIndex(m_frameIndex));
}

void PlayerWidget::pause() {
    if (!m_playing && !m_timer.isActive()) return;
    m_timer.stop();
    setPlaying(false);
}

void PlayerWidget::togglePlay() {
    if (m_playing) pause();
    else play();
}

void PlayerWidget::prevFrame() {
    if (m_reader && m_frameIndex > 0)
        showFrame(m_frameIndex - 1);
}

void PlayerWidget::nextFrame() {
    if (m_reader && m_frameIndex < m_frameCount - 1)
        showFrame(m_frameIndex + 1);
}

void PlayerWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(26, 26, 26));

    if (m_currentImage.isNull()) {
        p.setPen(QColor(100, 100, 100));
        p.drawText(rect(), Qt::AlignCenter, "拖动 GIF 文件到此处 或 文件 → 打开");
        return;
    }

    QSize imgSize = m_currentImage.size();
    QSize viewSize = size();
    int maxRender = 1200;
    if (viewSize.width() > maxRender) viewSize.setWidth(maxRender);
    if (viewSize.height() > maxRender) viewSize.setHeight(maxRender);

    imgSize.scale(viewSize, Qt::KeepAspectRatio);

    QImage scaled = m_currentImage.scaled(imgSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPoint pos((width() - imgSize.width()) / 2, (height() - imgSize.height()) / 2);
    p.drawImage(pos, scaled);
}

void PlayerWidget::resizeEvent(QResizeEvent *) {
    update();
}

void PlayerWidget::wheelEvent(QWheelEvent *event) {
    int delta = event->angleDelta().y();
    if (delta > 0) prevFrame();
    else if (delta < 0) nextFrame();
}

void PlayerWidget::setPlaying(bool playing) {
    if (m_playing == playing) return;
    m_playing = playing;
    emit playingChanged(m_playing);
}

int PlayerWidget::frameDurationForDisplayIndex(int index) const {
    if (!m_reader || index < 0 || index >= m_frameCount) return 100;
    int src = m_srcMapper ? m_srcMapper(index) : index;
    const auto &durations = m_reader->info().durations;
    if (src < 0 || src >= static_cast<int>(durations.size())) return 100;
    return std::max(10, durations[src]);
}

int PlayerWidget::lastPlayableFrame() const {
    if (m_frameCount <= 0) return 0;
    return std::clamp(m_stopFrame, 0, m_frameCount - 1);
}
