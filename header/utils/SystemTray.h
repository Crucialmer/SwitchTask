#ifndef WIN_SWITCHER_SYSTEMTRAY_H
#define WIN_SWITCHER_SYSTEMTRAY_H

#include <QSystemTrayIcon>

class QMenu;
class MenuHeaderWidget;
class UpdateChecker;

#define sysTray SystemTray::instance()

class SystemTray : public QSystemTrayIcon {
public:
    SystemTray(const SystemTray&) = delete;
    SystemTray& operator=(const SystemTray&) = delete;

    static SystemTray& instance();

    static void showSettingsDialog();
    static void showUpdateDialog();
    void applyMenuTheme();
    static void applyMenuThemeTo(QMenu* m);

private:
    explicit SystemTray(QWidget* parent = nullptr);
    void setMenu(QWidget* parent = nullptr);

    QMenu* menu = nullptr; // 托盘菜单（圆角 + 毛玻璃，随主题刷新）
    QMenu* menu_monitor = nullptr; // Display Monitor 子菜单
    MenuHeaderWidget* headerWidget = nullptr; // 顶部专属品牌状态卡片
    UpdateChecker* updateChecker = nullptr; // 更新静默检查（B+D：启动 + 24h）
    bool updateReminderPending = false; // 更新提醒存续（未处理）：托盘图标单击兜底路径用；被消费时清除
    bool lastToastWasUpdate = false;    // 最近一条通知是否为“更新可用”：messageClicked 的归属判定
};

#endif // WIN_SWITCHER_SYSTEMTRAY_H
