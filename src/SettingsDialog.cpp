#include "SettingsDialog.h"
#include "utils/MacTitleBar.h"
#include "utils/MacControls.h"
#include "utils/ConfigManager.h"
#include "utils/Startup.h"
#include "utils/Theme.h"
#include "utils/setWindowBlur.h"
#include "utils/Util.h"
#include "utils/SystemTray.h"
#include <QStyleOption>
#include <QPainter>
#include <QSignalBlocker>
#include <QScreen>
#include <QIcon>
#include <QScrollArea>
#include <QScrollBar>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("macWindowFrame");
    setMinimumSize(500, 560);
    resize(520, 580);
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

    // 1. 顶部固定标题栏
    m_titleBar = new MacTitleBar(QStringLiteral("SwitchTask 设置"), this);
    mainLayout->addWidget(m_titleBar);

    // 2. 中部可平滑滚动卡片区域（即使在超高 DPI、大字号缩放或极小屏幕下也绝不压缩文字高度）
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("settingsScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (scrollArea->viewport()) {
        scrollArea->viewport()->setAutoFillBackground(false);
    }

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName("settingsScrollContent");
    scrollContent->setAttribute(Qt::WA_TranslucentBackground);

    auto* contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(18, 12, 18, 12);
    contentLayout->setSpacing(10);

    // 顶部 Hero 品牌区域（40px 高清 Squircle 图标 + 品牌名 + 版本徽标 + 8K 副标题）
    auto* heroWidget = new QWidget(scrollContent);
    auto* heroLayout = new QHBoxLayout(heroWidget);
    heroLayout->setContentsMargins(4, 2, 4, 4);
    heroLayout->setSpacing(12);

    auto* heroIconLabel = new QLabel(heroWidget);
    const qreal dpr = devicePixelRatioF();
    QPixmap iconPm = QIcon(":/img/icon.ico").pixmap(QSize(qRound(40 * dpr), qRound(40 * dpr)));
    iconPm.setDevicePixelRatio(dpr);
    heroIconLabel->setPixmap(iconPm);
    heroIconLabel->setFixedSize(40, 40);
    heroLayout->addWidget(heroIconLabel, 0, Qt::AlignVCenter);

    auto* heroTextLayout = new QVBoxLayout();
    heroTextLayout->setContentsMargins(0, 0, 0, 0);
    heroTextLayout->setSpacing(3);

    auto* heroTitleRow = new QHBoxLayout();
    heroTitleRow->setContentsMargins(0, 0, 0, 0);
    heroTitleRow->setSpacing(8);

    auto* heroTitle = new QLabel(QStringLiteral("SwitchTask"), heroWidget);
    heroTitle->setObjectName("label_heroTitle");
    const QString verStr = [] {
        auto v = qApp ? qApp->applicationVersion() : QStringLiteral("1.0");
        if (v.isEmpty()) v = QStringLiteral("1.0");
        if (v.count('.') > 1 && v.endsWith(".0")) v.chop(2);
        return QString("v%1").arg(v);
    }();
    auto* heroBadge = new QLabel(verStr, heroWidget);
    heroBadge->setObjectName("badge_blue");

    heroTitleRow->addWidget(heroTitle);
    heroTitleRow->addWidget(heroBadge);
    heroTitleRow->addStretch();

    auto* heroSub = new QLabel(QStringLiteral("macOS 风格窗口切换与交互中心 · 8K 超清感知引擎"), heroWidget);
    heroSub->setObjectName("label_dim");
    heroSub->setWordWrap(true);

    heroTextLayout->addLayout(heroTitleRow);
    heroTextLayout->addWidget(heroSub);
    heroLayout->addLayout(heroTextLayout, 1);

    contentLayout->addWidget(heroWidget);

    // 卡片 1：系统开机启动（带有 macOS 胶囊滑动开关 MacToggleSwitch）
    auto* cardStartup = new QFrame(scrollContent);
    cardStartup->setObjectName("macCard");
    auto* layoutStartup = new QHBoxLayout(cardStartup);
    layoutStartup->setContentsMargins(14, 12, 14, 12);
    layoutStartup->setSpacing(12);

    auto* badgeStartup = new MacCategoryBadge(MacCategoryBadge::Rocket, QColor(0x00, 0x7A, 0xFF), cardStartup);
    auto* textStartupLayout = new QVBoxLayout();
    textStartupLayout->setContentsMargins(0, 0, 0, 0);
    textStartupLayout->setSpacing(4);

    auto* lblStartup = new QLabel(QStringLiteral("登录 Windows 时自动启动"), cardStartup);
    lblStartup->setObjectName("label_section");
    auto* descStartup = new QLabel(QStringLiteral("以当前权限在系统后台静默驻留，开机即刻就绪"), cardStartup);
    descStartup->setObjectName("label_dim");
    descStartup->setWordWrap(true);

    textStartupLayout->addWidget(lblStartup);
    textStartupLayout->addWidget(descStartup);

    m_startupSwitch = new MacToggleSwitch(cardStartup);

    layoutStartup->addWidget(badgeStartup, 0, Qt::AlignVCenter);
    layoutStartup->addLayout(textStartupLayout, 1);
    layoutStartup->addWidget(m_startupSwitch, 0, Qt::AlignVCenter);
    contentLayout->addWidget(cardStartup);

    // 卡片 2：显示器偏好（带有 macOS 原生分段选择器 MacSegmentedControl）
    auto* cardMonitor = new QFrame(scrollContent);
    cardMonitor->setObjectName("macCard");
    auto* layoutMonitor = new QVBoxLayout(cardMonitor);
    layoutMonitor->setContentsMargins(14, 12, 14, 12);
    layoutMonitor->setSpacing(10);

    auto* headerMonitorRow = new QHBoxLayout();
    headerMonitorRow->setContentsMargins(0, 0, 0, 0);
    headerMonitorRow->setSpacing(10);

    auto* badgeMonitor = new MacCategoryBadge(MacCategoryBadge::Monitor, QColor(0x58, 0x56, 0xD6), cardMonitor);
    auto* textMonitorLayout = new QVBoxLayout();
    textMonitorLayout->setContentsMargins(0, 0, 0, 0);
    textMonitorLayout->setSpacing(4);

    auto* lblMonitor = new QLabel(QStringLiteral("切换器呼出显示器偏好"), cardMonitor);
    lblMonitor->setObjectName("label_section");
    auto* descMonitor = new QLabel(QStringLiteral("决定按 Alt+Tab 时切换面板弹出的目标屏幕（支持多屏混合 DPI）"), cardMonitor);
    descMonitor->setObjectName("label_dim");
    descMonitor->setWordWrap(true);

    textMonitorLayout->addWidget(lblMonitor);
    textMonitorLayout->addWidget(descMonitor);
    headerMonitorRow->addWidget(badgeMonitor, 0, Qt::AlignVCenter);
    headerMonitorRow->addLayout(textMonitorLayout, 1);

    m_monitorSeg = new MacSegmentedControl({
        QStringLiteral("跟随鼠标所在屏幕 (推荐)"),
        QStringLiteral("固定在主显示器")
    }, cardMonitor);

    layoutMonitor->addLayout(headerMonitorRow);
    layoutMonitor->addWidget(m_monitorSeg);
    contentLayout->addWidget(cardMonitor);

    // 卡片 3：外观主题（带有 macOS 原生分段选择器 MacSegmentedControl）
    auto* cardTheme = new QFrame(scrollContent);
    cardTheme->setObjectName("macCard");
    auto* layoutTheme = new QVBoxLayout(cardTheme);
    layoutTheme->setContentsMargins(14, 12, 14, 12);
    layoutTheme->setSpacing(10);

    auto* headerThemeRow = new QHBoxLayout();
    headerThemeRow->setContentsMargins(0, 0, 0, 0);
    headerThemeRow->setSpacing(10);

    auto* badgeTheme = new MacCategoryBadge(MacCategoryBadge::Palette, QColor(0xFF, 0x95, 0x00), cardTheme);
    auto* textThemeLayout = new QVBoxLayout();
    textThemeLayout->setContentsMargins(0, 0, 0, 0);
    textThemeLayout->setSpacing(4);

    auto* lblTheme = new QLabel(QStringLiteral("界面外观主题"), cardTheme);
    lblTheme->setObjectName("label_section");
    auto* descTheme = new QLabel(QStringLiteral("切换器面板、托盘弹出窗口与自绘气泡主题"), cardTheme);
    descTheme->setObjectName("label_dim");
    descTheme->setWordWrap(true);

    textThemeLayout->addWidget(lblTheme);
    textThemeLayout->addWidget(descTheme);
    headerThemeRow->addWidget(badgeTheme, 0, Qt::AlignVCenter);
    headerThemeRow->addLayout(textThemeLayout, 1);

    m_themeSeg = new MacSegmentedControl({
        QStringLiteral("跟随系统 (自动)"),
        QStringLiteral("浅色模式"),
        QStringLiteral("深色模式")
    }, cardTheme);

    layoutTheme->addLayout(headerThemeRow);
    layoutTheme->addWidget(m_themeSeg);
    contentLayout->addWidget(cardTheme);

    // 卡片 4：高级配置管理
    auto* cardConfig = new QFrame(scrollContent);
    cardConfig->setObjectName("macCard");
    auto* layoutConfig = new QHBoxLayout(cardConfig);
    layoutConfig->setContentsMargins(14, 12, 14, 12);
    layoutConfig->setSpacing(12);

    auto* badgeConfig = new MacCategoryBadge(MacCategoryBadge::Gear, QColor(0x8E, 0x8E, 0x93), cardConfig);
    auto* textConfigLayout = new QVBoxLayout();
    textConfigLayout->setContentsMargins(0, 0, 0, 0);
    textConfigLayout->setSpacing(4);

    auto* lblConfig = new QLabel(QStringLiteral("高级自定义配置 (config.ini)"), cardConfig);
    lblConfig->setObjectName("label_section");
    auto* descConfig = new QLabel(QStringLiteral("深度定制标题字体族、字号、字重与滚轮交互参数"), cardConfig);
    descConfig->setObjectName("label_dim");
    descConfig->setWordWrap(true);

    textConfigLayout->addWidget(lblConfig);
    textConfigLayout->addWidget(descConfig);

    m_editConfigBtn = new QPushButton(QStringLiteral("打开配置..."), cardConfig);
    m_editConfigBtn->setObjectName("btn_config");

    layoutConfig->addWidget(badgeConfig, 0, Qt::AlignVCenter);
    layoutConfig->addLayout(textConfigLayout, 1);
    layoutConfig->addWidget(m_editConfigBtn, 0, Qt::AlignVCenter);
    contentLayout->addWidget(cardConfig);

    contentLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // 3. 底部固定控制按钮行
    auto* bottomBar = new QWidget(this);
    bottomBar->setObjectName("settingsBottomBar");
    auto* bottomRow = new QHBoxLayout(bottomBar);
    bottomRow->setContentsMargins(18, 10, 18, 14);
    bottomRow->setSpacing(10);

    m_checkUpdateBtn = new QPushButton(QStringLiteral("检查更新..."), bottomBar);
    m_checkUpdateBtn->setObjectName("btn_recheck");
    m_doneBtn = new QPushButton(QStringLiteral("完成"), bottomBar);
    m_doneBtn->setObjectName("btn_done");

    bottomRow->addWidget(m_checkUpdateBtn);
    bottomRow->addStretch();
    bottomRow->addWidget(m_doneBtn);

    mainLayout->addWidget(bottomBar, 0);

    // 交互逻辑绑定
    connect(m_startupSwitch, &MacToggleSwitch::toggled, this, [](bool checked) {
        if (Startup::isOn() != checked) {
            Startup::toggle();
        }
    });

    connect(m_monitorSeg, &MacSegmentedControl::currentIndexChanged, this, [](int index) {
        CFG.setDisplayMonitor(index == 0 ? MouseMonitor : PrimaryMonitor);
    });

    connect(m_themeSeg, &MacSegmentedControl::currentIndexChanged, this, [this](int index) {
        const QString mode = (index == 0) ? QStringLiteral("auto")
                           : ((index == 1) ? QStringLiteral("light") : QStringLiteral("dark"));
        CFG.set("ThemeMode", mode);
        Theme::applyConfiguredScheme();
        this->applyTheme();
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
    update();
}

void SettingsDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void SettingsDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    setWindowBlur(reinterpret_cast<HWND>(winId()));
    Util::setWindowRoundCorner(reinterpret_cast<HWND>(winId()));

    applyTheme();

    // 刷新控件当前值（使用 QSignalBlocker 避免在展示时触发写配置与级联重绘）
    {
        const QSignalBlocker b1(m_startupSwitch);
        const QSignalBlocker b2(m_monitorSeg);
        const QSignalBlocker b3(m_themeSeg);

        m_startupSwitch->setChecked(Startup::isOn(), false);
        m_monitorSeg->setCurrentIndex(CFG.getDisplayMonitor() == MouseMonitor ? 0 : 1, false);

        const auto theme = CFG.getThemeMode();
        const int themeIdx = (theme == ThemeAuto) ? 0 : ((theme == ThemeLight) ? 1 : 2);
        m_themeSeg->setCurrentIndex(themeIdx, false);
    }

    // 自动定位至屏幕工作区右下角
    MacTitleBar::positionBottomRight(this, 24);
}
