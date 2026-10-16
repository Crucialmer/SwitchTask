#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

class MacTitleBar;
class MacToggleSwitch;
class MacSegmentedControl;

/// macOS 26 风格独立设置窗口：贴靠右下角托盘区，圆角磨砂通透质感，8K 超清感知
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void applyTheme();

protected:
    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();

    MacTitleBar* m_titleBar = nullptr;
    MacToggleSwitch* m_startupSwitch = nullptr;
    MacSegmentedControl* m_monitorSeg = nullptr;
    MacSegmentedControl* m_themeSeg = nullptr;
    QPushButton* m_editConfigBtn = nullptr;
    QPushButton* m_checkUpdateBtn = nullptr;
    QPushButton* m_doneBtn = nullptr;
};

#endif // WIN_SWITCHER_SETTINGSDIALOG_H
