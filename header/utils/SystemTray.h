#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QActionGroup>
#include <QTimer>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>
#include <QPointer>
#include "Startup.h"
#include "ConfigManager.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "SettingsDialog.h"
#include "MacToast.h"
#include "Theme.h"
#include "setWindowBlur.h"

#define sysTray SystemTray::instance()

/// 状态呼吸圆点：抗锯齿矢量圆点绘制，默认 Apple Traffic Green (#34C759)
class StatusDot : public QWidget {
public:
    explicit StatusDot(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(10, 10);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void setColor(const QColor& c) {
        if (dotColor != c) {
            dotColor = c;
            update();
        }
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(QRectF(1.75, 1.75, 6.5, 6.5));
    }
private:
    QColor dotColor = QColor(0x34, 0xC7, 0x59);
};

/// macOS 风格菜单顶栏专属品牌状态卡片（Header Card，对标 Tcursor 2.5.2）
/// 第一行：左侧 18px 连续曲率方圆角应用图标 + 加粗排版 SwitchTask 品牌名，右侧精细磨砂半透明版本徽标胶囊；
/// 第二行：绿/橙实时呼吸圆点 + Bold 700 实时状态文本；
/// 容器具备独立温润衬底与边框，支持悬停高亮与单击直达配置/设置。
class MenuHeaderWidget : public QWidget {
public:
    explicit MenuHeaderWidget(const QString& version, QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("menuHeaderWidget");
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("点击打开设置..."));

        auto* rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(4, 3, 4, 3);
        rootLayout->setSpacing(0);

        card = new QFrame(this);
        card->setObjectName("menuHeaderCard");
        card->setCursor(Qt::PointingHandCursor);
        card->installEventFilter(this);

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);
        cardLayout->setSpacing(5);

        // 第一行：[18px 图标] + SwitchTask 品牌名 (Bold 700) + [版本徽标小胶囊]
        auto* line1 = new QHBoxLayout();
        line1->setContentsMargins(0, 0, 0, 0);
        line1->setSpacing(8);

        auto* iconLabel = new QLabel(card);
        iconLabel->setObjectName("menuHeaderIcon");
        iconLabel->setPixmap(QIcon(":/img/icon.ico").pixmap(18, 18));
        iconLabel->setFixedSize(18, 18);
        iconLabel->setStyleSheet("background: transparent;");
        iconLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        line1->addWidget(iconLabel, 0, Qt::AlignVCenter);

        auto* titleLabel = new QLabel(QStringLiteral("SwitchTask"), card);
        titleLabel->setObjectName("menuHeaderTitle");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        line1->addWidget(titleLabel, 0, Qt::AlignVCenter);

        line1->addStretch();

        auto* versionLabel = new QLabel(version, card);
        versionLabel->setObjectName("menuHeaderVersion");
        versionLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        line1->addWidget(versionLabel, 0, Qt::AlignVCenter);

        cardLayout->addLayout(line1);

        // 第二行：绿/橙实时呼吸圆点 + Bold 700 粗体运行状态文本
        auto* line2 = new QHBoxLayout();
        line2->setContentsMargins(0, 0, 0, 0);
        line2->setSpacing(6);

        statusDot = new StatusDot(card);
        line2->addWidget(statusDot, 0, Qt::AlignVCenter);

        statusLabel = new QLabel(QStringLiteral("运行中 · 快捷切换就绪"), card);
        statusLabel->setObjectName("menuHeaderStatusText");
        statusLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        line2->addWidget(statusLabel, 1, Qt::AlignVCenter);

        cardLayout->addLayout(line2);

        rootLayout->addWidget(card);
        setFixedHeight(58);
    }

    void updateStatus(const QString& text, const QColor& dotColor = QColor(0x34, 0xC7, 0x59)) {
        if (statusLabel) statusLabel->setText(text);
        if (statusDot) statusDot->setColor(dotColor);
    }

    QSize sizeHint() const override {
        return QSize(268, 58);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == card && event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                handleClick();
                return true;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            handleClick();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    void handleClick();

    QFrame* card = nullptr;
    StatusDot* statusDot = nullptr;
    QLabel* statusLabel = nullptr;
};

class SystemTray : public QSystemTrayIcon {
public:
    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    static SystemTray& instance() {
        // 第一次调用时 才会构造
        static SystemTray instance;
        return instance;
    }

private:
    explicit SystemTray(QWidget* parent = nullptr) : QSystemTrayIcon(parent) {
        setIcon(QIcon(":/img/icon.ico"));
        setMenu(parent);
        setToolTip(IsUserAnAdmin() ? "SwitchTask (管理员)" : "SwitchTask");
        // 启动后延迟预热 Startup 缓存，首次弹出托盘菜单不再同步等 schtasks 子进程
        QTimer::singleShot(3000, this, [] { Startup::isOn(true); });

        // 更新提示（用户裁决 B+D）：启动静默检查一次 + 每 24h 复查；发现新版本 → 托盘气泡，点击气泡进入更新对话框。
        // 静默纪律：检查失败只记日志；提醒后仍由用户点击 Update 才下载安装（不静默替换文件）
        updateChecker = new UpdateChecker(this);
        connect(updateChecker, &UpdateChecker::newVersionFound, this, [this](const QString& ver) {
            updateReminderPending = true; // 提醒存续（未处理）：托盘图标单击兜底路径用
            lastToastWasUpdate = true;    // 归属标记：本条通知即“更新可用”
            MacToast::showToast(
                QStringLiteral("发现新版本"),
                QStringLiteral("SwitchTask v%1 现已推出 — 点击查看详情并更新。").arg(ver),
                MacToast::Info,
                7000,
                [this] {
                    updateReminderPending = false;
                    lastToastWasUpdate = false;
                    showUpdateDialog();
                }
            );
        });
        connect(this, &QSystemTrayIcon::messageClicked, this, [this] {
            // 只响应“更新可用”那条通知的点击（lastToastWasUpdate 由各通知发出处维护；
            // 经验：气泡超时后点击不再传回应用——可交互窗口 ≈ showMessage 超时参数）
            if (lastToastWasUpdate) {
                lastToastWasUpdate = false;
                updateReminderPending = false;
                showUpdateDialog();
            }
        });
        // 兜底：错过气泡（超时/被关）后不再无路可走——提醒存续期间单击托盘图标同样进入更新对话框
        connect(this, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger && updateReminderPending) {
                updateReminderPending = false;
                showUpdateDialog();
            }
        });
        updateChecker->start();
    }

    void setMenu(QWidget* parent = nullptr) {
        menu = new QMenu(parent);
        // macOS 风格托盘菜单：圆角 + 毛玻璃（与切换器面板同一设计语言）。
        // 圆角需无边框 + 透明窗口承载 QSS 绘制；具体样式由 applyMenuTheme() 随深浅色模式刷新
        menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu->setAttribute(Qt::WA_TranslucentBackground);

        menu->setMinimumWidth(268);

        // macOS 风格顶部品牌状态卡片（Header Card，对标 Tcursor 2.5.2）
        const QString verStr = [] {
            auto v = qApp ? qApp->applicationVersion() : QStringLiteral("1.25");
            if (v.isEmpty()) v = QStringLiteral("1.25");
            if (v.count('.') > 1 && v.endsWith(".0")) v.chop(2);
            return QString("v%1").arg(v);
        }();
        headerWidget = new MenuHeaderWidget(verStr, menu);
        auto* headerAction = new QWidgetAction(menu);
        headerAction->setDefaultWidget(headerWidget);

        auto* act_update = new QAction("检查更新...", menu);
        auto* act_settings = new QAction("设置...", menu);
        auto* act_startup = new QAction("开机自启动", menu);
        menu_monitor = new QMenu("显示器设置", menu);
        menu_monitor->setMinimumWidth(220);
        menu_monitor->setWindowFlags(menu_monitor->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu_monitor->setAttribute(Qt::WA_TranslucentBackground);
        auto* act_quit = new QAction("退出 SwitchTask", menu);
        act_quit->setShortcut(QKeySequence("Ctrl+Q"));

        connect(act_update, &QAction::triggered, this, [this] {
            updateReminderPending = false; // 菜单已打开更新：提醒视为已处理
            showUpdateDialog(); // 与“更新可用”通知点击共用同一入口
        });

        connect(act_settings, &QAction::triggered, this, [] {
            showSettingsDialog();
        });
        connect(&CFG, &ConfigManager::configEdited, this, [this] {
            lastToastWasUpdate = false;
        });

        act_startup->setCheckable(true);
        // triggered vs toggled: setChecked() will emit `toggled`, but not `triggered` (which is pure user action)
        connect(act_startup, &QAction::triggered, this, [this](bool checked) {
            Q_UNUSED(checked);
            lastToastWasUpdate = false;
            Startup::toggle();
        });
        // aboutToShow 时查询，反映真实状态（isOn 带 60s 缓存 + 启动时预热，菜单弹出不再等 schtasks 子进程）
        connect(menu, &QMenu::aboutToShow, act_startup, [act_startup] {
            act_startup->setChecked(Startup::isOn());
        });

        // menu_monitor
        {
            auto* monitorGroup = new QActionGroup(menu_monitor);
            monitorGroup->addAction("主显示器")->setData(PrimaryMonitor);
            monitorGroup->addAction("跟随鼠标")->setData(MouseMonitor);

            const auto actions = monitorGroup->actions();
            for (auto* act: actions)
                act->setCheckable(true);

            Q_ASSERT(actions.size() == DisplayMonitor::EnumCount);
            menu_monitor->addActions(actions);

            // 动态响应配置文件修改
            connect(menu_monitor, &QMenu::aboutToShow, this, [monitorGroup] {
                qDebug() << "menu_monitor aboutToShow";
                const auto actions = monitorGroup->actions();
                actions[CFG.getDisplayMonitor()]->setChecked(true);
            });

            connect(monitorGroup, &QActionGroup::triggered, this, [this](QAction* act) {
                lastToastWasUpdate = false;
                auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
                CFG.setDisplayMonitor(monitor);
            });
        }

        connect(act_quit, &QAction::triggered, qApp, &QApplication::quit);

        menu->addAction(headerAction);
        menu->addSeparator(); // 顶部品牌卡片与功能区微光分隔线
        menu->addAction(act_update);
        menu->addAction(act_settings);
        menu->addSeparator(); // 逻辑分组 1：应用维护与配置
        menu->addAction(act_startup);
        menu->addMenu(menu_monitor);
        menu->addSeparator(); // 逻辑分组 2：系统行为与退出
        menu->addAction(act_quit);
        this->setContextMenu(menu);

        // 毛玻璃在【每次弹出时】应用到当时的 HWND：QMenu::popup 会先按目标屏幕创建窗口，
        // 若构造期提前 winId()，原生窗口会被钉在启动屏幕的 DPI 上（多屏混合缩放场景）；重放幂等
        connect(menu, &QMenu::aboutToShow, menu, [this] {
            setWindowBlur(reinterpret_cast<HWND>(menu->winId()));
            if (headerWidget) {
                headerWidget->updateStatus(QStringLiteral("运行中 · 快捷切换就绪"));
            }
        });
        connect(menu_monitor, &QMenu::aboutToShow, menu_monitor, [this] {
            setWindowBlur(reinterpret_cast<HWND>(menu_monitor->winId()));
        });

        // 主题刷新：系统深浅色切换 / 配置修改后即时重新着色
        applyMenuTheme();
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            applyMenuTheme();
        });
        connect(&CFG, &ConfigManager::configEdited, this, [this] {
            Theme::applyConfiguredScheme();
            applyMenuTheme();
        });
    }

    /// 应用/刷新托盘菜单的深浅色样式（主菜单 + Display Monitor 子菜单两个窗体）
    void applyMenuTheme() {
        applyMenuThemeTo(menu);
        applyMenuThemeTo(menu_monitor);
    }

    /// 单个菜单窗体：应用深浅色样式表（毛玻璃在每次弹出时应用到当时的 HWND，见 setMenu()）
    static void applyMenuThemeTo(QMenu* m) {
        if (!m)
            return;
        m->setStyleSheet(Theme::menuStyleSheet());
    }

public:
    /// 打开 macOS 26 风格设置窗口
    static void showSettingsDialog() {
        static QPointer<SettingsDialog> s_settingsDialog;
        if (!s_settingsDialog) {
            s_settingsDialog = new SettingsDialog;
        }
        s_settingsDialog->show();
        s_settingsDialog->raise();
        s_settingsDialog->activateWindow();
    }

    /// 打开更新对话框（托盘菜单与“更新可用”通知点击共用同一入口）
    static void showUpdateDialog() {
        // !不要用 static 局部变量对象：程序结束时析构可能访问已清理的 qApp 资源（"No style available without QApplication!"）
        static auto* dlg = new UpdateDialog;
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    }

private:
    QMenu* menu = nullptr; // 托盘菜单（圆角 + 毛玻璃，随主题刷新）
    QMenu* menu_monitor = nullptr; // Display Monitor 子菜单
    MenuHeaderWidget* headerWidget = nullptr; // 顶部专属品牌状态卡片
    UpdateChecker* updateChecker = nullptr; // 更新静默检查（B+D：启动 + 24h）
    bool updateReminderPending = false; // 更新提醒存续（未处理）：托盘图标单击兜底路径用；被消费时清除
    bool lastToastWasUpdate = false;    // 最近一条通知是否为“更新可用”：messageClicked 的归属判定
};

inline void MenuHeaderWidget::handleClick() {
    QWidget* topMenu = this;
    while (topMenu && topMenu->parentWidget()) {
        topMenu = topMenu->parentWidget();
        if (auto* m = qobject_cast<QMenu*>(topMenu)) {
            m->close();
            break;
        }
    }
    SystemTray::showSettingsDialog();
}

#endif //WIN_SWITCHER_SYSTEMTRAY_H
