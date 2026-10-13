#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QActionGroup>
#include <QTimer>
#include "Startup.h"
#include "ConfigManager.h"
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
            // !对于UI窗体，不要用static变量(`static UpdateDialog dlg;`)，要么局部变量，要么指针
            // ! static局部变量会在程序结束时析构 ! 析构时可能访问qApp资源，而qApp由于`qApp->quit()`已经被清理
            // !导致报错（"No style available without QApplication!"）
            static auto* dlg = new UpdateDialog;
            dlg->show();
        });

        connect(act_settings, &QAction::triggered, this, [] {
            cfg.editConfigFile();
        });
        connect(&cfg, &ConfigManager::configEdited, this, [this] {
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
                actions[cfg.getDisplayMonitor()]->setChecked(true);
            });

            connect(monitorGroup, &QActionGroup::triggered, this, [this](QAction* act) {
                auto monitor = static_cast<DisplayMonitor>(act->data().toInt());
                cfg.setDisplayMonitor(monitor);
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
        connect(&cfg, &ConfigManager::configEdited, this, [this] {
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

private:
    QMenu* menu = nullptr; // 托盘菜单（圆角 + 毛玻璃，随主题刷新）
    QMenu* menu_monitor = nullptr; // Display Monitor 子菜单
};

#endif //WIN_SWITCHER_SYSTEMTRAY_H
