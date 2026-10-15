#include "utils/TaskbarWheelHooker.h"
#include <QTimer>
#include <QTime>
#include <atomic>
#include "utils/uiautomation.h"
#include "utils/AppUtil.h"
#include "utils/Util.h"

namespace {
    /// 背压：任务栏滚轮事件的“在途”计数（从钩子排队起、到 GUI 处理完毕止）。
    /// 快速连滚 / GUI 忙时，逐事件排队会线性堆积——而处理时刻才重读光标下的元素，
    /// 积压会把“某个时刻的意图”按停手后的光标位置解释，表现为“停手后仍连续切换、
    /// 且可能切到别的应用”。阈值 1：已有在途事件时丢弃新事件（饱和才生效，正常速率无行为变化）。
    std::atomic_int g_pendingWheel{0};
}

/// 低级钩子回调：只做廉价判定与事件缓存，重活（UIA查询/路径解析/切窗）全部由主线程
/// processWheelEvent() 异步处理——WH_MOUSE_LL 回调超时（默认1s）会被系统静默跳过，
/// 表现为滚轮偶发失灵，因此回调内不做任何耗时操作。
LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEWHEEL) {
        auto* data = (MSLLHOOKSTRUCT*) lParam;
        HWND topLevelHwnd = Util::topWindowFromPoint(data->pt);
        if (Util::isTaskbarWindow(topLevelHwnd)) {
            auto delta = (short) HIWORD(data->mouseData);
            // 背压（饱和才生效）：已有在途事件 → 不排队，照常放行给系统
            if (g_pendingWheel.load(std::memory_order_relaxed) >= 1)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            // 仅记录事件，QueuedConnection 交给 GUI 线程处理（instance 生命周期与进程一致，判空兜底）
            if (TaskbarWheelHooker::instance) {
                g_pendingWheel.fetch_add(1, std::memory_order_relaxed);
                emit TaskbarWheelHooker::instance->rawWheelEvent(delta > 0, topLevelHwnd);
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

/// 在 GUI 线程执行原先钩子回调内的全部耗时逻辑
void TaskbarWheelHooker::processWheelEvent(bool isUp, HWND topLevelHwnd) {
    // 背压计数归还：函数任意 return 路径都必须归还（RAII 一次性覆盖）
    struct PendingGuard {
        ~PendingGuard() { g_pendingWheel.fetch_sub(1, std::memory_order_relaxed); }
    } pendingGuard;
    auto delta = isUp ? 120 : -120;
    qDebug() << "--- Taskbar Mouse Wheel" << (delta > 0 ? "↑" : "↓");
    auto el = UIAutomation::getElementUnderMouse(); // RVO优化 不会调用移动构造; // this line may bomb TODO 也许可以改成遍历元素
    // UIA 属性查询是跨进程 RPC：各属性只取一次、全程复用（原先同一事件最多 7 次 RPC）
    auto cls = el.getClassName();
    auto automationId = el.getAutomationId();
    auto name = el.getName();
    qDebug() << delta << cls << automationId << name;
    if (cls == "CEF-OSC-WIDGET") { // Nvidia Overlay
        // 当所有窗口最小化后，会出现这种情况，但是焦点和前台都不是他，离谱
        qDebug() << "detect CEF, try active taskbar";
        Util::switchToWindow(topLevelHwnd, true); // 只能通过变焦到Taskbar使Element正常检测
        el = std::move(UIAutomation::getElementUnderMouse());
        cls = el.getClassName();
        automationId = el.getAutomationId();
        name = el.getName();
        qDebug() << (cls != "CEF-OSC-WIDGET" ? "successful!" : "failed");
    }
    if (cls == "Taskbar.TaskListButtonAutomationPeer") {
        // TODO 在副屏 有时候会是"TaskbarFrame"
        if (!automationId.startsWith(QStringLiteral("Appid: "))) { // 非预期来源：宁可不动，也不要用被截断的字符串匹配到错误的分组
            qDebug() << "Unexpected AutomationId, skipped:" << automationId;
            return;
        }
        auto appid = automationId.mid(QStringLiteral("Appid: ").size());
        int windows = 0;
        const auto Dash = QStringLiteral(" - ");
        if (auto dashIdx = name.lastIndexOf(Dash); dashIdx != -1) { // "Clash for Windows - 1 个运行窗口"
            std::stringstream ss(name.mid(dashIdx + Dash.size()).toStdString());
            ss >> windows; // 从标题手动解析真实窗口数量，程序内部由于过滤逻辑的存在，可能不准确
            name = name.left(dashIdx);
        }
        auto exePath = AppUtil::getExePathFromAppIdOrName(appid, name);
        emit tabWheelEvent(exePath, isUp, windows);
    }
}

TaskbarWheelHooker::TaskbarWheelHooker() {
    if (instance) {
        qCritical() << "Only one TaskbarWheelHooker can be installed!";
        return;
    }
    instance = this;
    AppUtil::getExePathFromAppIdOrName(); // cache

    // 原生钩子线程的 rawWheelEvent → GUI 线程的 processWheelEvent（跨线程必须 QueuedConnection）
    connect(this, &TaskbarWheelHooker::rawWheelEvent, this, &TaskbarWheelHooker::processWheelEvent,
            Qt::QueuedConnection);

    // 单次安装低级鼠标钩子：回调内部只在监听到 WM_MOUSEWHEEL 时做极轻量窗口判定（非滚轮消息零开销直接放行）。
    // 彻底移除每 50ms 唤醒一次 CPU 的死循环轮询与频繁装卸钩子，消除笔记本电量消耗与 Win32 钩子链抖动。
    h_mouse = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC) mouseProc, GetModuleHandle(nullptr), 0);
    if (!h_mouse) {
        qCritical() << "Failed to install taskbar mouse hook!";
    }

    // 任务栏空闲超时：滚动停止 800ms 后发出 leaveTaskbar 信号，清理同组窗口顺序
    auto* idleTimer = new QTimer(this);
    idleTimer->setSingleShot(true);
    idleTimer->setInterval(800);
    connect(idleTimer, &QTimer::timeout, this, [this]() {
        emit leaveTaskbar();
    });
    connect(this, &TaskbarWheelHooker::rawWheelEvent, idleTimer, [idleTimer]() {
        idleTimer->start();
    });
}

TaskbarWheelHooker::~TaskbarWheelHooker() {
    if (h_mouse) {
        UnhookWindowsHookEx(h_mouse);
        h_mouse = nullptr;
        qDebug() << "MouseHooker uninstalled";
    }
    TaskbarWheelHooker::instance = nullptr;
    UIAutomation::cleanup();
}
