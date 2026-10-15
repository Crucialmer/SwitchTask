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
#include <QLabel>
#include "Startup.h"
#include "ConfigManager.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "Theme.h"
#include "setWindowBlur.h"

#define sysTray SystemTray::instance()

/// macOS 风格菜单顶栏品牌卡片：[Icon] SwitchTask      v1.19
class MenuHeaderWidget : public QWidget {
public:
    explicit MenuHeaderWidget(const QString& version, QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("menuHeaderWidget");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(12, 7, 12, 6);
        layout->setSpacing(8);

        auto* iconLabel = new QLabel(this);
        iconLabel->setObjectName("menuHeaderIcon");
        iconLabel->setPixmap(QIcon(":/img/icon.ico").pixmap(18, 18));
        iconLabel->setFixedSize(18, 18);
        iconLabel->setStyleSheet("background: transparent;");
        layout->addWidget(iconLabel);

        auto* titleLabel = new QLabel("SwitchTask", this);
        titleLabel->setObjectName("menuHeaderTitle");
        layout->addWidget(titleLabel);

        layout->addStretch();

        auto* versionLabel = new QLabel(version, this);
        versionLabel->setObjectName("menuHeaderVersion");
        layout->addWidget(versionLabel);
    }
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
        setToolTip(IsUserAnAdmin() ? "SwitchTask (admin)" : "SwitchTask");
        // 启动后延迟预热 Startup 缓存，首次弹出托盘菜单不再同步等 schtasks 子进程
        QTimer::singleShot(3000, this, [] { Startup::isOn(true); });

        // 更新提示（用户裁决 B+D）：启动静默检查一次 + 每 24h 复查；发现新版本 → 托盘气泡，点击气泡进入更新对话框。
        // 静默纪律：检查失败只记日志；提醒后仍由用户点击 Update 才下载安装（不静默替换文件）
        updateChecker = new UpdateChecker(this);
        connect(updateChecker, &UpdateChecker::newVersionFound, this, [this](const QString& ver) {
            updateReminderPending = true; // 提醒存续（未处理）：托盘图标单击兜底路径用
            lastToastWasUpdate = true;    // 归属标记：本条通知即“更新可用”
            // 可交互时长取系统上限 ~30s：实测系统只在其超时前派发点击（超时后气泡即使仍在屏上，
            // 点击也仅解散气泡、不传回应用）——久置气泡点击不可依赖，另配托盘单击兜底（见 activated）
            showMessage("Update Available",
                        QString("SwitchTask v%1 is available — click to review.").arg(ver),
                        QSystemTrayIcon::Information, 30000);
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

        // macOS 风格顶部品牌状态卡片（Header Card）
        const QString verStr = [] {
            auto v = qApp ? qApp->applicationVersion() : QStringLiteral("1.19");
            if (v.isEmpty()) v = QStringLiteral("1.19");
            if (v.count('.') > 1 && v.endsWith(".0")) v.chop(2);
            return QString("v%1").arg(v);
        }();
        auto* headerAction = new QWidgetAction(menu);
        headerAction->setDefaultWidget(new MenuHeaderWidget(verStr, menu));

        auto* act_update = new QAction("Check for Updates...", menu);
        auto* act_settings = new QAction("Settings...", menu);
        auto* act_startup = new QAction("Start with Windows", menu);
        menu_monitor = new QMenu("Display Monitor", menu);
        menu_monitor->setWindowFlags(menu_monitor->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu_monitor->setAttribute(Qt::WA_TranslucentBackground);
        auto* act_quit = new QAction("Quit SwitchTask", menu);
        act_quit->setShortcut(QKeySequence("Ctrl+Q"));

        connect(act_update, &QAction::triggered, this, [this] {
            updateReminderPending = false; // 菜单已打开更新：提醒视为已处理
            showUpdateDialog(); // 与“更新可用”通知点击共用同一入口
        });

        connect(act_settings, &QAction::triggered, this, [] {
            CFG.editConfigFile();
        });
        connect(&CFG, &ConfigManager::configEdited, this, [this] {
            lastToastWasUpdate = false; // 本条非“更新可用”：messageClicked 归属修正（托盘单击兜底不受影响）
            this->showMessage("Config Edited", "auto reloaded");
        });

        act_startup->setCheckable(true);
        // triggered vs toggled: setChecked() will emit `toggled`, but not `triggered` (which is pure user action)
        connect(act_startup, &QAction::triggered, this, [this](bool checked) {
            lastToastWasUpdate = false; // 同上：本条通知非“更新可用”
            Startup::toggle();
            if (Startup::isOn() == checked) // toggle 后缓存已重建（invalidateCache），此处直读缓存不阻塞
                this->showMessage("auto Startup mode", checked ? "ON √" : "OFF ×");
            else
                this->showMessage("Action Failed", "Failed to change Startup mode", Warning);
        });
        // aboutToShow 时查询，反映真实状态（isOn 带 60s 缓存 + 启动时预热，菜单弹出不再等 schtasks 子进程）
        connect(menu, &QMenu::aboutToShow, act_startup, [act_startup] {
            act_startup->setChecked(Startup::isOn());
        });

        // menu_monitor
        {
            auto* monitorGroup = new QActionGroup(menu_monitor);
            monitorGroup->addAction("Primary Monitor")->setData(PrimaryMonitor);
            monitorGroup->addAction("Mouse Monitor")->setData(MouseMonitor);

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
                lastToastWasUpdate = false; // 同上
                auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
                CFG.setDisplayMonitor(monitor);
                this->showMessage("Display Monitor Changed", act->text());
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

    /// 打开更新对话框（托盘菜单与“更新可用”通知点击共用同一入口）
    static void showUpdateDialog() {
        // !不要用 static 局部变量对象：程序结束时析构可能访问已清理的 qApp 资源（"No style available without QApplication!"）
        static auto* dlg = new UpdateDialog;
        dlg->show();
    }

private:
    QMenu* menu = nullptr; // 托盘菜单（圆角 + 毛玻璃，随主题刷新）
    QMenu* menu_monitor = nullptr; // Display Monitor 子菜单
    UpdateChecker* updateChecker = nullptr; // 更新静默检查（B+D：启动 + 24h）
    bool updateReminderPending = false; // 更新提醒存续（未处理）：托盘图标单击兜底路径用；被消费时清除
    bool lastToastWasUpdate = false;    // 最近一条通知是否为“更新可用”：messageClicked 的归属判定
};

#endif //WIN_SWITCHER_SYSTEMTRAY_H
