#include "editor_panel.h"

#include "gif_reader.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

static QWidget *makeInfoRow(const QString &label, QLabel *valueLabel) {
    auto *w = new QWidget();
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 2, 0, 2);
    lay->addWidget(new QLabel(label), 0);
    lay->addStretch();
    lay->addWidget(valueLabel, 0);
    return w;
}

static QWidget *makeActionRow(QPushButton *button) {
    auto *w = new QWidget();
    auto *lay = new QHBoxLayout(w);
    lay->setContentsMargins(0, 2, 0, 2);
    lay->addWidget(button);
    return w;
}

EditorPanel::EditorPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 8);
    lay->setSpacing(8);

    m_fileLabel = new QLabel("未打开文件");
    m_fileLabel->setStyleSheet("font-weight:bold; font-size:13px;");
    m_fileLabel->setWordWrap(true);
    lay->addWidget(m_fileLabel);

    auto *rangeTitle = new QLabel("当前范围");
    rangeTitle->setStyleSheet("color:#888; font-size:10px; margin-top:4px;");
    lay->addWidget(rangeTitle);

    m_rangeStartLabel = new QLabel("—");
    m_rangeEndLabel = new QLabel("—");
    m_exportSizeLabel = new QLabel("—");

    lay->addWidget(makeInfoRow("起始帧", m_rangeStartLabel));
    lay->addWidget(makeInfoRow("结束帧", m_rangeEndLabel));
    lay->addWidget(makeInfoRow("导出大小", m_exportSizeLabel));

    auto *actLabel = new QLabel("优化动作");
    actLabel->setStyleSheet("color:#888; font-size:10px; margin-top:12px;");
    lay->addWidget(actLabel);

    m_skipBtn = new QPushButton("抽帧…");
    m_speedBtn = new QPushButton("倍速…");
    m_scaleBtn = new QPushButton("缩放…");
    m_colorBtn = new QPushButton("颜色…");

    lay->addWidget(makeActionRow(m_skipBtn));
    lay->addWidget(makeActionRow(m_speedBtn));
    lay->addWidget(makeActionRow(m_scaleBtn));
    lay->addWidget(makeActionRow(m_colorBtn));
    lay->addStretch();

    setActionButtonsEnabled(false);

    connect(m_skipBtn, &QPushButton::clicked, this, &EditorPanel::applySkip);
    connect(m_speedBtn, &QPushButton::clicked, this, &EditorPanel::applySpeed);
    connect(m_scaleBtn, &QPushButton::clicked, this, &EditorPanel::applyScale);
    connect(m_colorBtn, &QPushButton::clicked, this, &EditorPanel::applyColor);
}

void EditorPanel::setReader(GifReader *reader) {
    m_reader = reader;
    setActionButtonsEnabled(reader != nullptr);

    if (!reader) {
        m_fileLabel->setText("未打开文件");
        m_rangeStartLabel->setText("—");
        m_rangeEndLabel->setText("—");
        m_exportSizeLabel->setText("—");
        return;
    }

    const auto &info = reader->info();
    m_lastSkip = 2;
    m_lastSpeed = 1.0;
    m_lastScale = 1.0;
    m_lastColor = 256;
    m_fileLabel->setText(QFileInfo(info.filePath).fileName()
                         + QString("\n%1 MB  |  %2 帧  |  %3×%4")
                               .arg(info.fileSize / 1024.0 / 1024.0, 0, 'f', 1)
                               .arg(info.frameCount)
                               .arg(info.width)
                               .arg(info.height));
}

void EditorPanel::setRangeFromSlider(int left, int right) {
    m_rangeStartLabel->setText(QString::number(left));
    m_rangeEndLabel->setText(QString::number(right));
}

void EditorPanel::setEstimatedSizeText(const QString &text) {
    m_exportSizeLabel->setText(text);
}

void EditorPanel::applySkip() {
    if (!m_reader) return;

    QDialog dialog(this);
    dialog.setWindowTitle("抽帧");

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *spin = new QSpinBox(&dialog);
    spin->setRange(2, 999);
    spin->setValue(m_lastSkip);
    spin->setToolTip("每 N 帧保留 1 帧");
    form->addRow("抽帧间隔", spin);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *applyBtn = buttons->addButton("应用", QDialogButtonBox::AcceptRole);
    applyBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_lastSkip = spin->value();
        emit skipApplied(m_lastSkip);
    }
}

void EditorPanel::applySpeed() {
    if (!m_reader) return;

    QDialog dialog(this);
    dialog.setWindowTitle("倍速");

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *spin = new QDoubleSpinBox(&dialog);
    spin->setRange(0.1, 10.0);
    spin->setSingleStep(0.1);
    spin->setDecimals(1);
    spin->setValue(m_lastSpeed);
    form->addRow("倍速", spin);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *applyBtn = buttons->addButton("应用", QDialogButtonBox::AcceptRole);
    applyBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_lastSpeed = spin->value();
        emit speedApplied(m_lastSpeed);
    }
}

void EditorPanel::applyScale() {
    if (!m_reader) return;

    QDialog dialog(this);
    dialog.setWindowTitle("缩放");

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *spin = new QDoubleSpinBox(&dialog);
    spin->setRange(0.1, 1.0);
    spin->setSingleStep(0.05);
    spin->setDecimals(2);
    spin->setValue(m_lastScale);
    form->addRow("缩放比例", spin);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *applyBtn = buttons->addButton("应用", QDialogButtonBox::AcceptRole);
    applyBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_lastScale = spin->value();
        emit scaleApplied(m_lastScale);
    }
}

void EditorPanel::applyColor() {
    if (!m_reader) return;

    QDialog dialog(this);
    dialog.setWindowTitle("颜色");

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *combo = new QComboBox(&dialog);
    combo->addItems({"256", "128", "64", "32"});
    int index = combo->findText(QString::number(m_lastColor));
    combo->setCurrentIndex(index >= 0 ? index : 0);
    form->addRow("颜色数", combo);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    auto *applyBtn = buttons->addButton("应用", QDialogButtonBox::AcceptRole);
    applyBtn->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        m_lastColor = combo->currentText().toInt();
        emit colorApplied(m_lastColor);
    }
}

void EditorPanel::setActionButtonsEnabled(bool enabled) {
    m_skipBtn->setEnabled(enabled);
    m_speedBtn->setEnabled(enabled);
    m_scaleBtn->setEnabled(enabled);
    m_colorBtn->setEnabled(enabled);
}
