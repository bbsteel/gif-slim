#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStringList>

#include <vector>

class GifReader;
class PlayerWidget;
class EditorPanel;
class RangeSlider;
class QDragEnterEvent;
class QDropEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public slots:
    void openFile(const QString &path = QString());
    void setRange(int start, int end);
    void applySkip(int everyN);
    void applySpeed(double factor);
    void applyScale(double factor);
    void applyColor(int numColors);
    int rangeStart() const;
    int rangeEnd() const;
    void saveAs(const QString &path);
    void cropToRange();
    void deleteRange();

private slots:
    void onFrameChanged(int displayIndex);
    void undo();
    void save();

private:
    void loadGif(const QString &path);
    void applyRangeOp(bool keepInRange);
    void rebuildActiveFrames(const std::vector<int> &newActive);
    int srcIndex(int displayIdx) const;
    void updateRangeState(int left, int right, bool jumpToStart);
    void updatePlayButton(bool playing);
    void refreshUiState();
    bool hasPendingChanges() const;
    QStringList buildChangeSummary() const;
    QString defaultSavePath() const;
    QString estimatedSizeText() const;
    int outputWidth() const;
    int outputHeight() const;
    bool exportGif(const QString &path, bool reloadAfterSave);
    static QString formatBytes(qint64 bytes);

    GifReader *m_reader = nullptr;
    PlayerWidget *m_player = nullptr;
    EditorPanel *m_editor = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_frameLabel = nullptr;
    QPushButton *m_playBtn = nullptr;
    QPushButton *m_undoBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    RangeSlider *m_rangeSlider = nullptr;

    std::vector<int> m_activeFrames;
    std::vector<int> m_prevActiveFrames;
    QString m_sourcePath;
    double m_speedFactor = 1.0;
    double m_scaleFactor = 1.0;
    int m_colorCount = 256;
};
