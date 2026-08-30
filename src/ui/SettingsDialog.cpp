#include "SettingsDialog.h"
#include <QFormLayout>
#include <QGroupBox>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("AgenticAI Settings");
    setFixedSize(440, 400);
    setupUi();
    applyDarkTheme();
}

void SettingsDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(18);

    // Title
    auto *titleLabel = new QLabel("⚙ Settings", this);
    titleLabel->setStyleSheet("color: #c0caf5; font-size: 20px; font-weight: 700; border: none;");
    mainLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("Configure model parameters and appearance", this);
    subtitleLabel->setStyleSheet("color: #565f89; font-size: 12px; border: none; margin-bottom: 4px;");
    mainLayout->addWidget(subtitleLabel);

    // --- Model Settings Group ---
    auto *modelGroup = new QGroupBox("Model Parameters", this);
    auto *modelLayout = new QFormLayout(modelGroup);
    modelLayout->setSpacing(14);
    modelLayout->setContentsMargins(16, 20, 16, 14);

    // Temperature
    auto *tempLayout = new QHBoxLayout();
    tempLayout->setSpacing(10);
    m_temperatureSlider = new QSlider(Qt::Horizontal, this);
    m_temperatureSlider->setRange(0, 200);
    m_temperatureSlider->setValue(70);
    m_tempValueLabel = new QLabel("0.70", this);
    m_tempValueLabel->setFixedWidth(40);
    m_tempValueLabel->setAlignment(Qt::AlignCenter);
    m_tempValueLabel->setStyleSheet("color: #7aa2f7; font-weight: bold; font-size: 13px; border: none;");
    tempLayout->addWidget(m_temperatureSlider, 1);
    tempLayout->addWidget(m_tempValueLabel);
    modelLayout->addRow("Temperature:", tempLayout);

    connect(m_temperatureSlider, &QSlider::valueChanged, this, [this](int val) {
        m_tempValueLabel->setText(QString::number(val / 100.0, 'f', 2));
    });

    // Repeat Penalty
    auto *repeatLayout = new QHBoxLayout();
    repeatLayout->setSpacing(10);
    m_repeatPenaltySlider = new QSlider(Qt::Horizontal, this);
    m_repeatPenaltySlider->setRange(100, 200);
    m_repeatPenaltySlider->setValue(110);
    m_repeatValueLabel = new QLabel("1.10", this);
    m_repeatValueLabel->setFixedWidth(40);
    m_repeatValueLabel->setAlignment(Qt::AlignCenter);
    m_repeatValueLabel->setStyleSheet("color: #7aa2f7; font-weight: bold; font-size: 13px; border: none;");
    repeatLayout->addWidget(m_repeatPenaltySlider, 1);
    repeatLayout->addWidget(m_repeatValueLabel);
    modelLayout->addRow("Repeat Penalty:", repeatLayout);

    connect(m_repeatPenaltySlider, &QSlider::valueChanged, this, [this](int val) {
        m_repeatValueLabel->setText(QString::number(val / 100.0, 'f', 2));
    });

    mainLayout->addWidget(modelGroup);

    // --- Appearance Group ---
    auto *appearGroup = new QGroupBox("Appearance", this);
    auto *appearLayout = new QFormLayout(appearGroup);
    appearLayout->setSpacing(14);
    appearLayout->setContentsMargins(16, 20, 16, 14);

    m_fontSizeCombo = new QComboBox(this);
    m_fontSizeCombo->addItems({"12", "13", "14", "15", "16"});
    m_fontSizeCombo->setCurrentText("14");
    appearLayout->addRow("Font Size:", m_fontSizeCombo);

    mainLayout->addWidget(appearGroup);

    mainLayout->addStretch();

    // --- Action Buttons ---
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setFixedWidth(100);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto *applyBtn = new QPushButton("Apply", this);
    applyBtn->setCursor(Qt::PointingHandCursor);
    applyBtn->setObjectName("applyButton");
    applyBtn->setFixedWidth(100);
    connect(applyBtn, &QPushButton::clicked, this, [this]() {
        emit settingsApplied(temperature(), repeatPenalty(), fontSize());
        accept();
    });

    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(applyBtn);
    mainLayout->addLayout(buttonLayout);
}

double SettingsDialog::temperature() const {
    return m_temperatureSlider ? m_temperatureSlider->value() / 100.0 : 0.7;
}

double SettingsDialog::repeatPenalty() const {
    return m_repeatPenaltySlider ? m_repeatPenaltySlider->value() / 100.0 : 1.1;
}

int SettingsDialog::fontSize() const {
    return m_fontSizeCombo ? m_fontSizeCombo->currentText().toInt() : 14;
}

void SettingsDialog::applyDarkTheme() {
    setStyleSheet(R"(
        QDialog {
            background-color: #131520;
            color: #c0caf5;
        }
        QGroupBox {
            color: #7aa2f7;
            font-weight: 600;
            font-size: 13px;
            border: 1px solid #2a2d4a;
            border-radius: 10px;
            margin-top: 14px;
            padding-top: 18px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 14px;
            padding: 0 8px;
            background-color: #131520;
        }
        QLabel {
            color: #a6adc8;
            font-size: 13px;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #2a2d4a;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #7aa2f7;
            width: 16px;
            height: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #89b4fa;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7aa2f7, stop:1 #5d85d4);
            border-radius: 2px;
        }
        QComboBox {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            border-radius: 8px;
            padding: 7px 14px;
            font-size: 13px;
        }
        QComboBox:hover {
            border: 1px solid #7aa2f7;
        }
        QComboBox::drop-down {
            border: none;
            padding-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            selection-background-color: rgba(122, 162, 247, 0.2);
            selection-color: #7aa2f7;
        }
        QPushButton {
            background-color: #1a1b2e;
            color: #c0caf5;
            border: 1px solid #2a2d4a;
            border-radius: 8px;
            padding: 9px 20px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:hover {
            background-color: #242640;
            border: 1px solid #7aa2f7;
            color: #7aa2f7;
        }
        QPushButton#applyButton {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7aa2f7, stop:1 #5d85d4);
            color: #0d0e15;
            border: none;
        }
        QPushButton#applyButton:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #89b4fa, stop:1 #7a9de0);
        }
    )");
}
