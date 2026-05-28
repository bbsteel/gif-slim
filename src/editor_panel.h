#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class GifReader;

class EditorPanel : public QWidget {
    Q_OBJECT

public:
    explicit EditorPanel(QWidget *parent = nullptr);

    void setReader(GifReader *reader);

public slots:
    void setRangeFromSlider(int left, int right);
    void setEstimatedSizeText(const QString &text);

signals:
    void skipApplied(int everyN);
    void speedApplied(double factor);
    void scaleApplied(double factor);
    void colorApplied(int numColors);

private:
    void applySkip();
    void applySpeed();
    void applyScale();
    void applyColor();
    void setActionButtonsEnabled(bool enabled);

    GifReader *m_reader = nullptr;

    QLabel *m_fileLabel = nullptr;
    QLabel *m_rangeStartLabel = nullptr;
    QLabel *m_rangeEndLabel = nullptr;
    QLabel *m_exportSizeLabel = nullptr;
    QPushButton *m_skipBtn = nullptr;
    QPushButton *m_speedBtn = nullptr;
    QPushButton *m_scaleBtn = nullptr;
    QPushButton *m_colorBtn = nullptr;

    int m_lastSkip = 2;
    double m_lastSpeed = 1.0;
    double m_lastScale = 1.0;
    int m_lastColor = 256;
};
