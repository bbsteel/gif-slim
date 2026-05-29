#include "range_slider.h"
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

RangeSlider::RangeSlider(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(52);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
}

void RangeSlider::setRange(int min, int max) {
    m_min = min; m_max = max;
    m_left = min; m_right = max;
    update();
}

void RangeSlider::setValues(int left, int right) {
    m_left = std::clamp(left, m_min, m_max);
    m_right = std::clamp(right, m_min, m_max);
    update();
    if (!m_silent) emit valuesChanged(m_left, m_right);
}

void RangeSlider::setPlayPosition(int pos) {
    m_playPos = pos;
    update();
}

int RangeSlider::posToValue(int x) const {
    int w = width() - 32;
    if (w <= 0) return m_min;
    double ratio = double(x - 16) / w;
    return m_min + int(ratio * (m_max - m_min) + 0.5);
}

int RangeSlider::valueToPos(int v) const {
    int w = width() - 32;
    if (m_max == m_min) return 16;
    return 16 + int(double(v - m_min) / (m_max - m_min) * w);
}

QRectF RangeSlider::leftHandleRect() const {
    int x = valueToPos(m_left);
    return QRectF(x - 8, 0, 16, height());
}

QRectF RangeSlider::rightHandleRect() const {
    int x = valueToPos(m_right);
    return QRectF(x - 8, 0, 16, height());
}

void RangeSlider::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int lx = valueToPos(m_left);
    int rx = valueToPos(m_right);
    int barY = 8;
    int barH = height() - 16;

    // 背景条 (灰色)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 60, 60));
    p.drawRoundedRect(QRectF(16, barY, width() - 32, barH), 4, 4);

    // 选中范围 (蓝色高亮)
    p.setBrush(QColor(80, 160, 255, 120));
    p.drawRoundedRect(QRectF(lx, barY, rx - lx, barH), 4, 4);

    // 选中范围边框
    p.setPen(QPen(QColor(100, 180, 255), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(lx, barY, rx - lx, barH), 4, 4);

    // 左把手
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(100, 180, 255));
    p.drawRoundedRect(QRectF(lx - 6, 0, 12, height()), 3, 3);

    // 右把手
    p.drawRoundedRect(QRectF(rx - 6, 0, 12, height()), 3, 3);

    // 把手上的竖线
    p.setPen(QPen(QColor(30, 30, 30), 2));
    p.drawLine(QPointF(lx, 12), QPointF(lx, height() - 12));
    p.drawLine(QPointF(rx, 12), QPointF(rx, height() - 12));

    // 播放位置指示线（黄色细线）
    if (m_playPos >= m_min && m_playPos <= m_max) {
        int px = valueToPos(m_playPos);
        p.setPen(QPen(QColor(255, 200, 50), 2));
        p.drawLine(QPointF(px, 2), QPointF(px, height() - 2));
    }

    // 帧号标注
    p.setPen(QColor(200, 200, 200));
    QFont f = font();
    f.setPixelSize(11);
    p.setFont(f);
    QString txt = QString("%1 - %2").arg(m_left).arg(m_right);
    p.drawText(QRectF(lx, barY, rx - lx, barH), Qt::AlignCenter, txt);
}

void RangeSlider::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) return;
    QPointF pos = event->position();

    if (leftHandleRect().contains(pos)) {
        m_dragging = -1;
    } else if (rightHandleRect().contains(pos)) {
        m_dragging = 1;
    } else {
        // Click on bar area → seek playback position
        int val = std::clamp(posToValue(int(pos.x())), m_min, m_max);
        emit seekRequested(val);
    }
}

void RangeSlider::mouseMoveEvent(QMouseEvent *event) {
    int val = std::clamp(posToValue(int(event->position().x())), m_min, m_max);

    if (m_dragging == -1) {
        m_left = std::min(val, m_right);
        update();
        emit valuesChanged(m_left, m_right);
    } else if (m_dragging == 1) {
        m_right = std::max(val, m_left);
        update();
        emit valuesChanged(m_left, m_right);
    } else {
        // hover 光标
        QPointF pos = event->position();
        if (leftHandleRect().contains(pos) || rightHandleRect().contains(pos))
            setCursor(Qt::SizeHorCursor);
        else
            setCursor(Qt::ArrowCursor);
    }
}

void RangeSlider::mouseReleaseEvent(QMouseEvent *) {
    if (m_dragging != 0) {
        emit rangeDragFinished(m_left, m_right);
    }
    m_dragging = 0;
}

void RangeSlider::resizeEvent(QResizeEvent *) {
    update();
}
