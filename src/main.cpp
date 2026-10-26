#include <QApplication>
#include <QFont>
#include <windows.h>
#include <QTimer>
#include <QMessageBox>
#include <qoperatingsystemversion.h>
#include <QStyleHints>
#include "UpdateDialog.h"
#include "widget.h"
#include "utils/winEventHook.h"
#include "utils/Util.h"
#include "utils/TaskbarWheelHooker.h"
#include "utils/KeyboardHooker.h"
#include "utils/ComInitializer.h"
#include "utils/SingleApp.h"
#include "utils/SystemTray.h"
#include "utils/Theme.h"

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // 免疫 DLL 劫持：移除非安全搜索路径，严格限定从 System32 与应用自身目录加载 DLL
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
    SetDllDirectory(L"");
    // 启用 Windows Per-Monitor V2 高 DPI 感知，支持 4K / 8K 显示器任意比例缩放（100% ~ 400%+）
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);
    QFont defaultAppFont("Microsoft YaHei UI");
    defaultAppFont.setStyleHint(QFont::SansSerif);
    QApplication::setFont(defaultAppFont);
#ifdef APP_VERSION
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION)); // UpdateDialog 更新检查依赖
#endif
    SingleApp singleApp("SwitchTask");
    if (singleApp.isRunning()) {
        qWarning() << "Another instance is running! Exit";
        QMessageBox::warning(nullptr, "Warning", "SwitchTask is already running!");
        return 0;
    }

    // 其实Qt内部已经初始化了，这里是保险起见
    ComInitializer com; // 初始化COM组件 for 主线程
    qDebug() << qt_error_string(S_OK); // just for fun

    qDebug() << "isUserAdmin" << IsUserAnAdmin();
    qDebug() << "System Version" << QOperatingSystemVersion::current().version();
    sysTray.show(); // show之后才能使用系统通知
    UpdateDialog::verifyUpdate(a); // 验证更新

    // 深浅色模式：默认跟随系统自动切换；config.ini 的 `ThemeMode` 可强制 light/dark（见 Theme.h）
    Theme::applyConfiguredScheme();
    qApp->setQuitOnLastWindowClosed(false);
    auto* winSwitcher = new Widget;
    winSwitcher->prepareListWidget(); // 优化：对ListWidget进行预先初始化，首次执行`setCurrentRow`特别耗时(472ms)
    Util::startIconPrefetch(); // 图标后台预取：新应用图标由工作线程提前提取，弹窗时无需首次同步等待

    QObject::connect(&a, &QApplication::aboutToQuit, []() {
        unhookWinEvent();
    });

    KeyboardHooker kbHooker(winSwitcher);
    TaskbarWheelHooker tbHooker;
    QObject::connect(&tbHooker, &TaskbarWheelHooker::tabWheelEvent,
                     winSwitcher, &Widget::rotateTaskbarWindowInGroup, Qt::QueuedConnection);
    // QueueConnection is important, ensure async, avoiding blocking the hook process
    QObject::connect(&tbHooker, &TaskbarWheelHooker::leaveTaskbar,
                     winSwitcher, &Widget::clearGroupWindowOrder, Qt::QueuedConnection);

    setWinEventHook([winSwitcher](DWORD event, HWND hwnd) {
        // 某些情况下，Hook拦截不到Alt+Tab（如VMware获取焦点且虚拟机开启时，即便focus在标题栏上）
        // （GPT建议RegisterRawInputDevices，不知道有没有效果，感觉比较危险）
        // 此时需要通过监控前台窗口检测系统的任务切换窗口唤出，并弹出本程序
        // 一旦焦点脱离VMWare，Hook就能正常工作，接下里就可以正常拦截Alt+Tab
        // 而再次连续按下TAB（Alt按住的情况下），也不会出现反复弹出系统任务切换窗口的情况（如果不进行Hook拦截就会这样，所以两种方法结合使用）
        if (event == EVENT_SYSTEM_FOREGROUND/* && hwnd == GetForegroundWindow()*/) { // 前台窗口变化 TODO 记录窗口关闭事件
            // 使用[原生]Alt+TAB呼出任务切换窗口时，会触发两次EVENT_SYSTEM_FOREGROUND事件
            // 第二次是在目标窗口已经切换到前台后触发的，非常诡异
            // ^1 可以用 GetForegroundWindow() || isAltPressed 来二次确认 （或者过滤相邻相同hwnd）

            // 对于Follower启动的CMD，由于Follower先行隐藏，会导致焦点先回落到上个窗口，再到新窗口 （但是用Win键打开的cmd没事）
            // 但是此时GetForegroundWindow()还是上个窗口，可能是更新不及时，所以 ^1-1 处的方案不可行
            // 问题不大，本程序hook了Alt+Tab，已经不会出现两次Event了
            winSwitcher->notifyForegroundChanged(hwnd, Widget::WinEvent);
            // ForegroundStaging貌似是辅助过渡动画
            // 检测 Alt 按下，防止误判 Win+Tab (任务视图)；类名查询移入条件（短路：仅前两项通过才发起系统调用）
            QString className;
            if (hwnd == GetForegroundWindow() && Util::isKeyPressed(VK_MENU) &&
                ((className = Util::getClassName(hwnd)) == "ForegroundStaging" /*|| className == "XamlExplorerHostIslandWindow"*/)) { // 任务切换窗口
                // 顺序是ForegroundStaging -> XamlExplorerHostIslandWindow，不需要都检测，否则会重复
                // 且：XamlExplorerHostIslandWindow 会导致误检测（某些系统版本，任务栏app窗口>1时，点击窗口）
                qDebug() << "任务切换 detected!" << className;
                // 非阻塞式异步重试，彻底移除主线程 Sleep(10)，避免冻结 Qt 事件循环与界面绘制
                winSwitcher->requestShow();
                for (int t = 1; t <= 4; ++t) {
                    QTimer::singleShot(15 * t, winSwitcher, [winSwitcher]() {
                        if (!winSwitcher->isForeground()) {
                            winSwitcher->requestShow();
                        }
                    });
                }
            }
        }
    });

    qInfo() << "@SwitchTask started!";
    return QApplication::exec();
}
