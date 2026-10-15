#include "SettingsDialog.h"
#include "utils/MacTitleBar.h"
#include "utils/ConfigManager.h"
#include "utils/Startup.h"
#include "utils/Theme.h"
#include "utils/setWindowBlur.h"
#include "utils/Util.h"
#include "utils/SystemTray.h"
#include <QButtonGroup>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(460, 480);
    setWindowTitle("SwitchTask 设置");

    setupUi();
    applyTheme();

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        applyTheme();
    });
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_windowFrame = new QFrame(this);
    m_windowFrame->setObjectName("macWindowFrame");

    auto* frameLayout = new QVBoxLayout(m_windowFrame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frameLayout->setSpacing(0);

    m_titleBar = new MacTitleBar(QStringLiteral("SwitchTask 设置"), this);
    frameLayout->addWidget(m_titleBar);

    auto* contentWidget = new QWidget(m_windowFrame);
    auto* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(18, 14, 18, 14);
    contentLayout->setSpacing(10);

    // 卡片 1：启动设置
    auto* cardStartup = new QFrame(contentWidget);
    cardStartup->setObjectName("macCard");
    auto* layoutStartup = new QVBoxLayout(cardStartup);
    layoutStartup->setContentsMargins(12, 10, 12, 10);
    layoutStartup->setSpacing(6);

    auto* lblStartup = new QLabel(QStringLiteral("系统开机启动"), cardStartup);
    lblStartup->setObjectName("label_section");
    m_startupCheck = new QCheckBox(QStringLiteral("登录 Windows 时自动启动 SwitchTask"), cardStartup);
    layoutStartup->addWidget(lblStartup);
    layoutStartup->addWidget(m_startupCheck);
    contentLayout->addWidget(cardStartup);

    // 卡片 2：显示器偏好
    auto* cardMonitor = new QFrame(contentWidget);
    cardMonitor->setObjectName("macCard");
    auto* layoutMonitor = new QVBoxLayout(cardMonitor);
    layoutMonitor->setContentsMargins(12, 10, 12, 10);
    layoutMonitor->setSpacing(6);

    auto* lblMonitor = new QLabel(QStringLiteral("窗口切换器呼出显示器"), cardMonitor);
    lblMonitor->setObjectName("label_section");
    m_monitorMouseRadio = new QRadioButton(QStringLiteral("跟随鼠标所在屏幕 (推荐)"), cardMonitor);
    m_monitorPrimaryRadio = new QRadioButton(QStringLiteral("固定在主显示器"), cardMonitor);

    auto* monitorGroup = new QButtonGroup(this);
    monitorGroup->addButton(m_monitorMouseRadio);
    monitorGroup->addButton(m_monitorPrimaryRadio);

    layoutMonitor->addWidget(lblMonitor);
    layoutMonitor->addWidget(m_monitorMouseRadio);
    layoutMonitor->addWidget(m_monitorPrimaryRadio);
    contentLayout->addWidget(cardMonitor);

    // 卡片 3：外观主题
    auto* cardTheme = new QFrame(contentWidget);
    cardTheme->setObjectName("macCard");
    auto* layoutTheme = new QVBoxLayout(cardTheme);
    layoutTheme->setContentsMargins(12, 10, 12, 10);
    layoutTheme->setSpacing(6);

    auto* lblTheme = new QLabel(QStringLiteral("界面外观主题"), cardTheme);
    lblTheme->setObjectName("label_section");

    auto* themeRadioLayout = new QHBoxLayout();
    themeRadioLayout->setContentsMargins(0, 0, 0, 0);
    themeRadioLayout->setSpacing(14);

    m_themeAutoRadio = new QRadioButton(QStringLiteral("跟随系统"), cardTheme);
    m_themeLightRadio = new QRadioButton(QStringLiteral("浅色模式"), cardTheme);
    m_themeDarkRadio = new QRadioButton(QStringLiteral("深色模式"), cardTheme);

    auto* themeGroup = new QButtonGroup(this);
    themeGroup->addButton(m_themeAutoRadio);
    themeGroup->addButton(m_themeLightRadio);
    themeGroup->addButton(m_themeDarkRadio);

    themeRadioLayout->addWidget(m_themeAutoRadio);
    themeRadioLayout->addWidget(m_themeLightRadio);
    themeRadioLayout->addWidget(m_themeDarkRadio);
    themeRadioLayout->addStretch();

    layoutTheme->addWidget(lblTheme);
    layoutTheme->addLayout(themeRadioLayout);
    contentLayout->addWidget(cardTheme);

    // 卡片 4：字体与排版说明
    auto* cardFont = new QFrame(contentWidget);
    cardFont->setObjectName("macCard");
    auto* layoutFont = new QVBoxLayout(cardFont);
    layoutFont->setContentsMargins(12, 10, 12, 10);
    layoutFont->setSpacing(4);

    auto* lblFont = new QLabel(QStringLiteral("排版与字体规范"), cardFont);
    lblFont->setObjectName("label_section");
    auto* lblFontDesc = new QLabel(QStringLiteral("全界面规范采用 Microsoft YaHei UI 配合 Bold 700 饱满排版；深浅色通透毛玻璃与 268px+ 宽屏自适应。"), cardFont);
    lblFontDesc->setObjectName("label_dim");
    lblFontDesc->setWordWrap(true);
    layoutFont->addWidget(lblFont);
    layoutFont->addWidget(lblFontDesc);
    contentLayout->addWidget(cardFont);

    contentLayout->addStretch();

    // 底部控制按钮行
    auto* bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 4, 0, 0);
    bottomRow->setSpacing(8);

    m_editConfigBtn = new QPushButton(QStringLiteral("打开 config.ini"), contentWidget);
    m_editConfigBtn->setObjectName("btn_config");
    m_checkUpdateBtn = new QPushButton(QStringLiteral("检查更新..."), contentWidget);
    m_checkUpdateBtn->setObjectName("btn_recheck");
    m_doneBtn = new QPushButton(QStringLiteral("完成"), contentWidget);
    m_doneBtn->setObjectName("btn_done");

    bottomRow->addWidget(m_editConfigBtn);
    bottomRow->addWidget(m_checkUpdateBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_doneBtn);

    contentLayout->addLayout(bottomRow);
    frameLayout->addWidget(contentWidget);
    mainLayout->addWidget(m_windowFrame);

    // 交互逻辑绑定（无 Win11 消息气泡打扰）
    connect(m_startupCheck, &QCheckBox::toggled, this, [](bool checked) {
        if (Startup::isOn() != checked) {
            Startup::toggle();
        }
    });

    connect(m_monitorMouseRadio, &QRadioButton::toggled, this, [](bool checked) {
        if (checked) {
            CFG.setDisplayMonitor(MouseMonitor);
        }
    });
    connect(m_monitorPrimaryRadio, &QRadioButton::toggled, this, [](bool checked) {
        if (checked) {
            CFG.setDisplayMonitor(PrimaryMonitor);
        }
    });

    connect(m_themeAutoRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            CFG.set("ThemeMode", "auto");
            Theme::applyConfiguredScheme();
            this->applyTheme();
        }
    });
    connect(m_themeLightRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            CFG.set("ThemeMode", "light");
            Theme::applyConfiguredScheme();
            this->applyTheme();
        }
    });
    connect(m_themeDarkRadio, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            CFG.set("ThemeMode", "dark");
            Theme::applyConfiguredScheme();
            this->applyTheme();
        }
    });

    connect(m_editConfigBtn, &QPushButton::clicked, this, [] {
        CFG.editConfigFile();
    });

    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this] {
        this->close();
        SystemTray::showUpdateDialog();
    });

    connect(m_doneBtn, &QPushButton::clicked, this, &QDialog::close);
}

void SettingsDialog::applyTheme() {
    setStyleSheet(Theme::macDialogStyleSheet());
}

void SettingsDialog::showEvent(QShowEvent*) {
    setWindowBlur(reinterpret_cast<HWND>(winId()));
    Util::setWindowRoundCorner(reinterpret_cast<HWND>(winId()));

    applyTheme();

    // 刷新控件当前值
    m_startupCheck->setChecked(Startup::isOn());
    m_monitorMouseRadio->setChecked(CFG.getDisplayMonitor() == MouseMonitor);
    m_monitorPrimaryRadio->setChecked(CFG.getDisplayMonitor() == PrimaryMonitor);

    const auto theme = CFG.getThemeMode();
    m_themeAutoRadio->setChecked(theme == ThemeAuto);
    m_themeLightRadio->setChecked(theme == ThemeLight);
    m_themeDarkRadio->setChecked(theme == ThemeDark);

    // 自动定位至屏幕工作区右下角
    MacTitleBar::positionBottomRight(this, 24);
}
