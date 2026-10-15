#ifndef WIN_SWITCHER_SETTINGSDIALOG_H
#define WIN_SWITCHER_SETTINGSDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

class MacTitleBar;

/// macOS 26 风格独立设置窗口：贴靠右下角托盘区，圆角磨砂通透质感
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void applyTheme();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();

    QFrame* m_windowFrame = nullptr;
    MacTitleBar* m_titleBar = nullptr;
    QCheckBox* m_startupCheck = nullptr;
    QRadioButton* m_monitorMouseRadio = nullptr;
    QRadioButton* m_monitorPrimaryRadio = nullptr;
    QRadioButton* m_themeAutoRadio = nullptr;
    QRadioButton* m_themeLightRadio = nullptr;
    QRadioButton* m_themeDarkRadio = nullptr;
    QPushButton* m_editConfigBtn = nullptr;
    QPushButton* m_checkUpdateBtn = nullptr;
    QPushButton* m_doneBtn = nullptr;
};

#endif // WIN_SWITCHER_SETTINGSDIALOG_H
