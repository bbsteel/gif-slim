#pragma once

#include <QWidget>

/// 宽条帧范围选择器 — 两端可拖动把手选择起止帧
class RangeSlider : public QWidget {
    Q_OBJECT

public:
    explicit RangeSlider(QWidget *parent = nullptr);

    void setRange(int min, int max);
    void setValues(int left, int right);
    void setPlayPosition(int pos);         // 播放当前位置指示线
    int leftValue() const { return m_left; }
    int rightValue() const { return m_right; }

signals:
    void valuesChanged(int left, int right);
    void rangeDragFinished(int left, int right);  // 把手拖动结束
    void seekRequested(int pos);       // 点击 bar 区域时请求跳转播放位置

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void resizeEvent(QResizeEvent *) override;

private:
    int m_min = 0, m_max = 100;
    int m_left = 0, m_right = 100;
    int m_playPos = -1;  // -1 = 不显示
    int m_dragging = 0; // 0=none, -1=left, 1=right

    QRectF leftHandleRect() const;
    QRectF rightHandleRect() const;
public:
    bool m_silent = false; // 设值时抑制信号，防止循环
private:
    int posToValue(int x) const;
    int valueToPos(int v) const;
};
