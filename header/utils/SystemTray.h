#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QActionGroup>
#include <QDateTime>
#include <QTimer>
#include "Startup.h"
#include "ConfigManager.h"
#include "UpdateChecker.h"
#include "UpdateDialog.h"
#include "Theme.h"
#include "setWindowBlur.h"

#define sysTray SystemTray::instance()

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
            updateToastAt = QDateTime::currentMSecsSinceEpoch();
            showMessage("Update Available",
                        QString("SwitchTask v%1 is available — click to review.").arg(ver),
                        QSystemTrayIcon::Information, 10000);
        });
        connect(this, &QSystemTrayIcon::messageClicked, this, [this] {
            // 仅当最近的通知是“更新可用”（≈气泡驻留时长内）才进入更新对话框；其它功能通知的点击无行为
            if (updateToastAt != 0 && QDateTime::currentMSecsSinceEpoch() - updateToastAt <= 12000) {
                updateToastAt = 0;
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

        auto* act_update = new QAction("Check for Updates", menu);
        auto* act_settings = new QAction("Settings", menu);
        auto* act_startup = new QAction("Start with Windows", menu);
        menu_monitor = new QMenu("Display Monitor", menu);
        menu_monitor->setWindowFlags(menu_monitor->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu_monitor->setAttribute(Qt::WA_TranslucentBackground);
        auto* act_quit = new QAction("Quit", menu);

        connect(act_update, &QAction::triggered, this, [] {
            showUpdateDialog(); // 与“更新可用”通知点击共用同一入口
        });

        connect(act_settings, &QAction::triggered, this, [] {
            CFG.editConfigFile();
        });
        connect(&CFG, &ConfigManager::configEdited, this, [this] {
            this->showMessage("Config Edited", "auto reloaded");
        });

        act_startup->setCheckable(true);
        // triggered vs toggled: setChecked() will emit `toggled`, but not `triggered` (which is pure user action)
        connect(act_startup, &QAction::triggered, this, [this](bool checked) {
            Startup::toggle();
            if (Startup::isOn() == checked) // toggle 后缓存已重建（invalidateCache），此处直读缓存不阻塞
                this->showMessage("auto Startup mode", checked ? "ON √" : "OFF ×");
            else
                this->showMessage("Action Failed", "Failed to change Startup mode", Warning);
        });
        // aboutToShow 时查询，反映真实状态（isOn 带 60s 缓存 + 启动时预热，菜单弹出不再等 schtasks 子进程）
        connect(menu, &QMenu::aboutToShow, act_startup, [act_startup] {
            act_startup->setChecked(Startup::isOn());

            static auto text = act_startup->text();
            if (IsUserAnAdmin() && !Startup::isOn_reg())
                act_startup->setText(text + "🔑️"); // 意味着接下来的操作需要管理员权限（操作schtask）
            else
                act_startup->setText(text);
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
                auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
                CFG.setDisplayMonitor(monitor);
                this->showMessage("Display Monitor Changed", act->text());
            });
        }

        connect(act_quit, &QAction::triggered, qApp, &QApplication::quit);

        menu->addAction(act_update);
        menu->addAction(act_settings);
        menu->addAction(act_startup);
        menu->addMenu(menu_monitor);
        menu->addSeparator(); // 结构分隔：功能性操作与退出分开
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
    qint64 updateToastAt = 0; // 最近一次“更新可用”通知的时间戳（messageClicked 据此判定是否进入更新对话框）
};

#endif //WIN_SWITCHER_SYSTEMTRAY_H
