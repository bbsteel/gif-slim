#include "mainwindow.h"

#include "editor_panel.h"
#include "gif_reader.h"
#include "gif_writer.h"
#include "player_widget.h"
#include "range_slider.h"

#include <QApplication>
#include <QDialog>
#include <QProgressDialog>
#include <QTimer>
#include <QDialogButtonBox>
#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("GIF Slim");
    resize(1100, 700);
    setAcceptDrops(true);

    auto *toolbar = addToolBar("控制");

    auto *openBtn = new QPushButton("📂 打开");
    openBtn->setFixedWidth(88);
    connect(openBtn, &QPushButton::clicked, this, [this]() { openFile(); });
    toolbar->addWidget(openBtn);

    m_saveBtn = new QPushButton("💾 保存");
    m_saveBtn->setEnabled(false);
    m_saveBtn->setStyleSheet("color: #4fc3f7;");
    toolbar->addWidget(m_saveBtn);

    toolbar->addSeparator();

    m_playBtn = new QPushButton("▶ 播放");
    m_playBtn->setFixedWidth(88);
    toolbar->addWidget(m_playBtn);

    auto *prevBtn = new QPushButton("⏮");
    prevBtn->setFixedWidth(36);
    toolbar->addWidget(prevBtn);

    auto *nextBtn = new QPushButton("⏭");
    nextBtn->setFixedWidth(36);
    toolbar->addWidget(nextBtn);

    m_frameLabel = new QLabel("0 / 0");
    m_frameLabel->setStyleSheet("font-size:11px; color:#aaa;");
    toolbar->addWidget(m_frameLabel);

    toolbar->addSeparator();

    auto *cropBtn = new QPushButton("裁剪至范围");
    toolbar->addWidget(cropBtn);
    auto *delBtn = new QPushButton("删除范围");
    toolbar->addWidget(delBtn);

    toolbar->addSeparator();
    m_undoBtn = new QPushButton("↩ 撤销");
    m_undoBtn->setEnabled(false);
    toolbar->addWidget(m_undoBtn);

    m_rangeSlider = new RangeSlider();
    m_rangeSlider->setFixedHeight(52);

    auto *splitter = new QSplitter(Qt::Horizontal);
    m_player = new PlayerWidget();
    splitter->addWidget(m_player);
    m_editor = new EditorPanel();
    m_editor->setFixedWidth(260);
    splitter->addWidget(m_editor);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    auto *central = new QWidget();
    auto *vlay = new QVBoxLayout(central);
    vlay->setContentsMargins(6, 2, 6, 2);
    vlay->setSpacing(4);
    vlay->addWidget(m_rangeSlider);
    vlay->addWidget(splitter);
    setCentralWidget(central);

    m_statusLabel = new QLabel("就绪 — 拖动 GIF 文件到窗口");
    statusBar()->addWidget(m_statusLabel);

    connect(m_playBtn, &QPushButton::clicked, m_player, &PlayerWidget::togglePlay);
    connect(prevBtn, &QPushButton::clicked, m_player, &PlayerWidget::prevFrame);
    connect(nextBtn, &QPushButton::clicked, m_player, &PlayerWidget::nextFrame);
    connect(m_player, &PlayerWidget::frameChanged, this, &MainWindow::onFrameChanged);
    connect(m_player, &PlayerWidget::playingChanged, this, &MainWindow::updatePlayButton);
    connect(cropBtn, &QPushButton::clicked, this, &MainWindow::cropToRange);
    connect(delBtn, &QPushButton::clicked, this, &MainWindow::deleteRange);
    connect(m_undoBtn, &QPushButton::clicked, this, &MainWindow::undo);
    connect(m_saveBtn, &QPushButton::clicked, this, &MainWindow::save);

    connect(m_editor, &EditorPanel::skipApplied, this, &MainWindow::applySkip);
    connect(m_editor, &EditorPanel::speedApplied, this, &MainWindow::applySpeed);
    connect(m_editor, &EditorPanel::scaleApplied, this, &MainWindow::applyScale);
    connect(m_editor, &EditorPanel::colorApplied, this, &MainWindow::applyColor);

    connect(m_rangeSlider, &RangeSlider::valuesChanged, this, [this](int left, int right) {
        updateRangeState(left, right, false);
    });
    connect(m_rangeSlider, &RangeSlider::rangeDragFinished, this, [this](int left, int right) {
        int cur = m_player->currentFrame();
        if (cur < left || cur > right) {
            m_player->showFrame(left);
        }
    });
    connect(m_rangeSlider, &RangeSlider::seekRequested, this, [this](int pos) {
        if (m_player->isPlaying()) m_player->pause();
        m_player->showFrame(pos);
    });
}

MainWindow::~MainWindow() {
    delete m_reader;
}

int MainWindow::srcIndex(int displayIdx) const {
    if (displayIdx >= 0 && displayIdx < static_cast<int>(m_activeFrames.size()))
        return m_activeFrames[displayIdx];
    return displayIdx;
}

void MainWindow::updateRangeState(int left, int right, bool jumpToStart) {
    if (!m_reader || m_activeFrames.empty()) return;

    left = std::clamp(left, 0, static_cast<int>(m_activeFrames.size()) - 1);
    right = std::clamp(right, left, static_cast<int>(m_activeFrames.size()) - 1);

    m_editor->setRangeFromSlider(left, right);
    m_player->setPlayRange(left, right);

    if (jumpToStart) {
        m_player->showFrame(left);
    }

    refreshUiState();
}

void MainWindow::updatePlayButton(bool playing) {
    m_playBtn->setText(playing ? "⏸ 暂停" : "▶ 播放");
}

void MainWindow::refreshUiState() {
    m_editor->setEstimatedSizeText(estimatedSizeText());
    m_saveBtn->setEnabled(hasPendingChanges());
    m_undoBtn->setEnabled(!m_prevActiveFrames.empty());
    updatePlayButton(m_player->isPlaying());
}

bool MainWindow::hasPendingChanges() const {
    if (!m_reader) return false;

    const auto &info = m_reader->info();
    if (m_speedFactor != 1.0 || m_scaleFactor != 1.0 || m_colorCount != 256) {
        return true;
    }

    if (static_cast<int>(m_activeFrames.size()) != info.frameCount) {
        return true;
    }

    for (int i = 0; i < info.frameCount; ++i) {
        if (m_activeFrames[i] != i) return true;
    }

    return false;
}

QStringList MainWindow::buildChangeSummary() const {
    QStringList lines;
    if (!m_reader) return lines;

    const auto &info = m_reader->info();
    lines << QString("导出帧数：%1 / %2").arg(m_activeFrames.size()).arg(info.frameCount);
    lines << QString("导出尺寸：%1 × %2").arg(outputWidth()).arg(outputHeight());

    if (m_speedFactor != 1.0) {
        lines << QString("倍速：%1x").arg(m_speedFactor, 0, 'f', 1);
    }
    if (m_scaleFactor != 1.0) {
        lines << QString("缩放：%1%").arg(int(m_scaleFactor * 100));
    }
    if (m_colorCount != 256) {
        lines << QString("颜色数：%1").arg(m_colorCount);
    }

    lines << QString("预计导出大小：%1").arg(estimatedSizeText());
    return lines;
}

QString MainWindow::defaultSavePath() const {
    if (m_sourcePath.isEmpty()) return "output.gif";

    QFileInfo info(m_sourcePath);
    return info.dir().filePath(info.completeBaseName() + "_edited.gif");
}

QString MainWindow::estimatedSizeText() const {
    if (!m_reader || m_activeFrames.empty()) return "—";

    const auto &info = m_reader->info();
    // When writing raw (no scale/color change), size scales linearly with frame count
    if (m_scaleFactor == 1.0 && m_colorCount == 256) {
        qint64 bytes = static_cast<qint64>(info.fileSize)
                     * static_cast<int>(m_activeFrames.size()) / info.frameCount;
        return formatBytes(bytes);
    }
    // Re-encode path: estimate based on frame size and LZW compression
    qint64 bytes = GifWriter::estimateSize(static_cast<int>(m_activeFrames.size()),
                                           info.width,
                                           info.height,
                                           m_colorCount,
                                           m_scaleFactor,
                                           1,
                                           m_speedFactor);
    return formatBytes(bytes);
}

int MainWindow::outputWidth() const {
    if (!m_reader) return 0;
    return std::max(1, int(m_reader->info().width * m_scaleFactor));
}

int MainWindow::outputHeight() const {
    if (!m_reader) return 0;
    return std::max(1, int(m_reader->info().height * m_scaleFactor));
}

bool MainWindow::exportGif(const QString &path, bool reloadAfterSave) {
    if (!m_reader || m_activeFrames.empty()) return false;

    QString outPath = path.trimmed();
    if (outPath.isEmpty()) return false;
    if (!outPath.endsWith(".gif", Qt::CaseInsensitive)) {
        outPath += ".gif";
    }

    m_player->pause();

    // Fast path: if nothing changed, just copy the original file
    bool unchanged = (m_scaleFactor == 1.0 && m_speedFactor == 1.0 && m_colorCount == 256
        && static_cast<int>(m_activeFrames.size()) == m_reader->info().frameCount);
    if (unchanged) {
        for (size_t i = 0; i < m_activeFrames.size(); i++) {
            if (m_activeFrames[i] != static_cast<int>(i)) { unchanged = false; break; }
        }
    }
    if (unchanged && m_sourcePath != outPath) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        bool copied = QFile::copy(m_sourcePath, outPath);
        QApplication::restoreOverrideCursor();
        if (copied) {
            m_statusLabel->setText(QString("已保存 → %1").arg(QFileInfo(outPath).fileName()));
            if (reloadAfterSave) {
                QString p = outPath;
                QTimer::singleShot(0, this, [this, p]() { loadGif(p); });
            }
            return true;
        }
    }

    // Smart path: when only cropping/skipping/speed (no scale or colour change),
    // copy original frame data directly — preserves partial frames and local palettes.
    bool rawOk = (m_scaleFactor == 1.0 && m_colorCount == 256 && !m_sourcePath.isEmpty());
    if (rawOk) {
        std::vector<int> durs;
        durs.reserve(m_activeFrames.size());
        for (int idx : m_activeFrames)
            durs.push_back(std::max(10, int(m_reader->info().durations[idx] / m_speedFactor)));

        QApplication::setOverrideCursor(Qt::WaitCursor);
        QProgressDialog rawProgress("正在保存 GIF（直通拷贝）...", QString(), 0, 0, this);
        rawProgress.setWindowModality(Qt::WindowModal);
        rawProgress.setMinimumDuration(0);
        rawProgress.show();
        QApplication::processEvents();

        qInfo() << "exportGif: raw copy" << m_activeFrames.size() << "frames";
        bool ok = GifWriter::writeRaw(outPath, m_sourcePath, m_activeFrames, durs);
        rawProgress.close();
        QApplication::restoreOverrideCursor();
        qInfo() << "exportGif: raw copy returned" << ok;
        if (ok) {
            m_statusLabel->setText(QString("已保存 %1 帧 → %2").arg(m_activeFrames.size()).arg(QFileInfo(outPath).fileName()));
            if (reloadAfterSave) {
                QString p = outPath;
                QTimer::singleShot(0, this, [this, p]() { loadGif(p); });
            }
            return true;
        }
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_statusLabel->setText("保存中...");

    QProgressDialog progress("正在保存 GIF...", "取消", 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setLabelText("正在分析颜色...");
    progress.show();
    QApplication::processEvents();

    auto *reader = m_reader;
    auto active = m_activeFrames;
    const double scale = m_scaleFactor;
    const double speed = m_speedFactor;
    const int colorCount = m_colorCount;

    auto frameSource = [reader, active, scale](int i) -> QImage {
        QImage img = reader->getFrame(active[i]);
        if (scale != 1.0) {
            int w = std::max(1, int(img.width() * scale));
            int h = std::max(1, int(img.height() * scale));
            img = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        // Color reduction is handled by GifWriter with a shared palette
        return img;
    };

    std::vector<int> durs;
    durs.reserve(active.size());
    for (int idx : active) {
        int d = reader->info().durations[idx];
        durs.push_back(std::max(10, int(d / speed)));
    }

    QFileInfo sourceInfo(m_sourcePath);
    QFileInfo outInfo(outPath);
    bool overwriteSource = sourceInfo.exists()
        && sourceInfo.absoluteFilePath() == outInfo.absoluteFilePath();

    QString backupPath;
    if (overwriteSource) {
        backupPath = m_sourcePath + ".bak";
        QFile::remove(backupPath);
        QFile::copy(m_sourcePath, backupPath);
    }

    qInfo() << "exportGif: writing" << active.size() << "frames to" << outPath;
    bool ok = GifWriter::write(outPath, frameSource, durs, 0, colorCount,
        [&progress](int done, int total) {
            if (progress.maximum() == 0) progress.setMaximum(total);
            progress.setLabelText(QString("正在写入帧 %1 / %2...").arg(done).arg(total));
            progress.setValue(done);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    qInfo() << "exportGif: write returned" << ok;
    progress.close();
    QApplication::restoreOverrideCursor();

    if (ok) {
        if (overwriteSource) QFile::remove(backupPath);
        m_statusLabel->setText(QString("已保存 %1 帧 → %2").arg(active.size()).arg(QFileInfo(outPath).fileName()));
        qInfo() << "exportGif: saved OK, reloadAfterSave=" << reloadAfterSave;
        if (reloadAfterSave) {
            QString p = outPath;
            QTimer::singleShot(0, this, [this, p]() { loadGif(p); });
        }
        return true;
    }

    if (overwriteSource) {
        QFile::remove(outPath);
        QFile::rename(backupPath, outPath);
        m_statusLabel->setText("保存失败，已恢复");
    } else {
        QFile::remove(outPath);
        m_statusLabel->setText("保存失败");
    }
    return false;
}

QString MainWindow::formatBytes(qint64 bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    return QString("%1 MB").arg(bytes / 1024.0 / 1024.0, 0, 'f', 1);
}

void MainWindow::rebuildActiveFrames(const std::vector<int> &newActive) {
    if (newActive.empty() || newActive == m_activeFrames) return;

    m_prevActiveFrames = m_activeFrames;
    m_activeFrames = newActive;

    int n = static_cast<int>(m_activeFrames.size());
    m_player->pause();
    m_player->setFrameCount(n);
    m_rangeSlider->setRange(0, n - 1);
    m_rangeSlider->setValues(0, n - 1);
}

void MainWindow::onFrameChanged(int displayIndex) {
    m_rangeSlider->setPlayPosition(displayIndex);
    m_frameLabel->setText(QString("%1 / %2").arg(displayIndex + 1).arg(m_activeFrames.size()));
}

void MainWindow::openFile(const QString &path) {
    QString p = path;
    if (p.isEmpty()) {
        p = QFileDialog::getOpenFileName(this, "打开 GIF", {}, "GIF (*.gif)");
    }
    if (!p.isEmpty()) loadGif(p);
}

void MainWindow::loadGif(const QString &path) {
    qInfo() << "loadGif:" << path;
    delete m_reader;
    m_reader = nullptr;
    m_statusLabel->setText("加载中...");
    QProgressDialog progress("正在加载 GIF...", QString(), 0, 0, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);
    progress.show();
    QApplication::processEvents();

    auto *reader = new GifReader(path);
    progress.close();
    if (!reader->isValid()) {
        qWarning() << "loadGif FAILED:" << path;
        delete reader;
        QMessageBox::warning(this, "错误", "无法打开 GIF:\n" + path);
        m_statusLabel->setText("就绪");
        return;
    }

    m_reader = reader;
    const auto &info = reader->info();

    m_activeFrames.clear();
    for (int i = 0; i < info.frameCount; ++i) {
        m_activeFrames.push_back(i);
    }
    m_prevActiveFrames.clear();
    m_speedFactor = 1.0;
    m_scaleFactor = 1.0;
    m_colorCount = 256;
    m_sourcePath = path;

    m_player->setReader(reader);
    m_player->setFrameCount(info.frameCount);
    m_player->setPlayRange(0, info.frameCount - 1);
    m_player->setSrcMapper([this](int idx) -> int { return srcIndex(idx); });

    m_editor->setReader(reader);
    m_rangeSlider->setRange(0, info.frameCount - 1);
    m_rangeSlider->setValues(0, info.frameCount - 1);

    m_statusLabel->setText(
        QString("%1  |  %2 帧  |  %3×%4  |  %5 MB")
            .arg(QFileInfo(path).fileName())
            .arg(info.frameCount)
            .arg(info.width).arg(info.height)
            .arg(info.fileSize / 1024.0 / 1024.0, 0, 'f', 1));
    qInfo() << "loadGif OK:" << info.frameCount << "frames";
}

void MainWindow::applyRangeOp(bool keepInRange) {
    if (!m_reader || m_activeFrames.empty()) return;

    int total = static_cast<int>(m_activeFrames.size());
    int start = std::clamp(rangeStart(), 0, total - 1);
    int end = std::clamp(rangeEnd(), start, total - 1);

    m_player->pause();

    std::vector<int> newActive;
    if (keepInRange) {
        for (int i = start; i <= end; ++i) {
            newActive.push_back(m_activeFrames[i]);
        }
    } else {
        for (int i = 0; i < start; ++i) {
            newActive.push_back(m_activeFrames[i]);
        }
        for (int i = end + 1; i < total; ++i) {
            newActive.push_back(m_activeFrames[i]);
        }
    }

    if (newActive.empty()) {
        QMessageBox::warning(this, "错误", "不能删除全部帧。");
        return;
    }

    if (newActive == m_activeFrames) {
        m_statusLabel->setText("当前范围没有产生新的变化");
        return;
    }

    rebuildActiveFrames(newActive);
    m_statusLabel->setText(QString("%1后剩余 %2 帧")
                               .arg(keepInRange ? "裁剪" : "删除")
                               .arg(newActive.size()));
}

void MainWindow::cropToRange() {
    applyRangeOp(true);
}

void MainWindow::deleteRange() {
    applyRangeOp(false);
}

void MainWindow::undo() {
    if (m_prevActiveFrames.empty()) return;

    std::vector<int> tmp = m_activeFrames;
    m_activeFrames = m_prevActiveFrames;
    m_prevActiveFrames = tmp;

    int n = static_cast<int>(m_activeFrames.size());
    m_player->pause();
    m_player->setFrameCount(n);
    m_rangeSlider->setRange(0, n - 1);
    m_rangeSlider->setValues(0, n - 1);

    m_statusLabel->setText("已撤销上一次范围调整");
}

void MainWindow::save() {
    if (!m_reader || !hasPendingChanges()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("保存 GIF");
    dialog.resize(520, 320);

    auto *layout = new QVBoxLayout(&dialog);
    auto *summaryTitle = new QLabel("将应用以下变更：", &dialog);
    layout->addWidget(summaryTitle);

    auto *summary = new QPlainTextEdit(&dialog);
    summary->setReadOnly(true);
    summary->setPlainText(buildChangeSummary().join("\n"));
    summary->setMinimumHeight(140);
    layout->addWidget(summary);

    auto *pathTitle = new QLabel("导出文件名：", &dialog);
    layout->addWidget(pathTitle);

    auto *pathEdit = new QLineEdit(defaultSavePath(), &dialog);
    layout->addWidget(pathEdit);

    auto *hint = new QLabel("使用原文件名会覆盖源文件；使用默认文件名可直接一键保存。", &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#888;");
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *saveNowBtn = buttons->addButton("一键保存", QDialogButtonBox::AcceptRole);
    saveNowBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;

    QString outPath = pathEdit->text().trimmed();
    if (!outPath.endsWith(".gif", Qt::CaseInsensitive))
        outPath += ".gif";

    if (QFile::exists(outPath)) {
        auto answer = QMessageBox::question(this, "确认覆盖",
            QString("文件 \"%1\" 已存在，是否覆盖？").arg(QFileInfo(outPath).fileName()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    exportGif(outPath, true);
}

void MainWindow::setRange(int start, int end) {
    if (!m_reader || m_activeFrames.empty()) return;
    m_rangeSlider->setValues(start, end);
}

int MainWindow::rangeStart() const {
    return m_rangeSlider->leftValue();
}

int MainWindow::rangeEnd() const {
    return m_rangeSlider->rightValue();
}

void MainWindow::applySkip(int everyN) {
    if (!m_reader || m_activeFrames.empty()) return;

    std::vector<int> newActive;
    for (size_t i = 0; i < m_activeFrames.size(); i += everyN) {
        newActive.push_back(m_activeFrames[i]);
    }
    rebuildActiveFrames(newActive);
    m_statusLabel->setText(QString("已应用抽帧：每 %1 帧保留 1 帧").arg(everyN));
}

void MainWindow::applySpeed(double factor) {
    if (!m_reader) return;
    m_speedFactor = factor;
    refreshUiState();
    m_statusLabel->setText(QString("已设置倍速：%1x").arg(factor, 0, 'f', 1));
}

void MainWindow::applyScale(double factor) {
    if (!m_reader) return;
    m_scaleFactor = factor;
    refreshUiState();
    m_statusLabel->setText(QString("已设置缩放：%1%").arg(int(factor * 100)));
}

void MainWindow::applyColor(int numColors) {
    if (!m_reader) return;
    m_colorCount = numColors;
    refreshUiState();
    m_statusLabel->setText(QString("已设置颜色数：%1").arg(numColors));
}

void MainWindow::saveAs(const QString &path) {
    exportGif(path, false);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        if (path.toLower().endsWith(".gif")) {
            loadGif(path);
            return;
        }
    }
}
