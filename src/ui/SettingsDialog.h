#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QSlider;
class QComboBox;
class QLabel;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    double temperature() const;
    double repeatPenalty() const;
    int fontSize() const;

signals:
    void settingsApplied(double temperature, double repeatPenalty, int fontSize);

private:
    void setupUi();
    void applyDarkTheme();

    QSlider *m_temperatureSlider{nullptr};
    QSlider *m_repeatPenaltySlider{nullptr};
    QComboBox *m_fontSizeCombo{nullptr};
    QLabel *m_tempValueLabel{nullptr};
    QLabel *m_repeatValueLabel{nullptr};
};

#endif // SETTINGSDIALOG_H
