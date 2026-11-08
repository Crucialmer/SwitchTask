#include "utils/TaskbarWheelHooker.h"
#include <QTimer>
#include <QTime>
#include <QStringView>
#include <atomic>
#include <future>
#include "utils/uiautomation.h"
#include "utils/AppUtil.h"
#include "utils/Util.h"

namespace {
    /// 背压：任务栏滚轮事件的“在途”计数（从钩子排队起、到 GUI 处理完毕止）。
    /// 快速连滚 / GUI 忙时，逐事件排队会线性堆积——而处理时刻才重读光标下的元素，
    /// 积压会把“某个时刻的意图”按停手后的光标位置解释，表现为“停手后仍连续切换、
    /// 且可能切到别的应用”。阈值 1：已有在途事件时丢弃新事件（饱和才生效，正常速率无行为变化）。
    std::atomic_int g_pendingWheel{0};

    /// 快速任务栏几何初筛：缓存主屏及副屏任务栏的外接矩形（物理像素）
    /// 避免全系统任意区域滚轮滚动时无差别触发 WindowFromPoint + GetAncestor + GetClassNameW
    struct TaskbarGeometryCache {
        ULONGLONG lastCheckedMs = 0;
        RECT rects[8];
        int count = 0;
    } g_taskbarGeo;

    bool isPointInTaskbarGeometry(const POINT& pt) {
        const ULONGLONG now = GetTickCount64();
        if (now - g_taskbarGeo.lastCheckedMs > 2000 || g_taskbarGeo.count == 0) {
            g_taskbarGeo.lastCheckedMs = now;
            g_taskbarGeo.count = 0;
            // 1. 主屏任务栏
            if (HWND hPrimary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
                if (IsWindowVisible(hPrimary) && GetWindowRect(hPrimary, &g_taskbarGeo.rects[g_taskbarGeo.count])) {
                    g_taskbarGeo.count++;
                }
            }
            // 2. 副屏任务栏
            HWND hSec = nullptr;
            while ((hSec = FindWindowExW(nullptr, hSec, L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
                if (g_taskbarGeo.count < 8 && IsWindowVisible(hSec)) {
                    if (GetWindowRect(hSec, &g_taskbarGeo.rects[g_taskbarGeo.count])) {
                        g_taskbarGeo.count++;
                    }
                }
            }
        }
        for (int i = 0; i < g_taskbarGeo.count; ++i) {
            if (PtInRect(&g_taskbarGeo.rects[i], pt))
                return true;
        }
        return false;
    }
}

/// 低级钩子回调：运行于专属独立消息线程，只做廉价判定与事件缓存，重活（UIA查询/路径解析/切窗）全部由主线程
/// processWheelEvent() 异步处理——WH_MOUSE_LL 回调超时会被系统静默跳过，
/// 独立线程保证永远不被 GUI 主线程的绘制或 I/O 阻塞。
LRESULT CALLBACK mouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        // 1. 双击屏蔽：复用常驻低级鼠标钩子，消除每次弹窗频繁 Set/UnhookWindowsHookEx
        if (Util::processClickShield(wParam, lParam))
            return 1;

        // 2. 任务栏滚轮切换
        if (wParam == WM_MOUSEWHEEL) {
            auto* data = (MSLLHOOKSTRUCT*) lParam;
            // 快速几何初筛：光标不在任何任务栏范围内时 O(1) 立即放行，避免触发跨进程系统调用
            if (isPointInTaskbarGeometry(data->pt)) {
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
    auto el = UIAutomation::getElementUnderMouse(); // RVO优化 不会调用移动构造
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
        if (!automationId.startsWith(QStringLiteral("Appid: "))) { // 非预期来源：跳过
            qDebug() << "Unexpected AutomationId, skipped:" << automationId;
            return;
        }
        auto appid = automationId.mid(QStringLiteral("Appid: ").size());
        int windows = 0;
        const auto Dash = QStringLiteral(" - ");
        if (auto dashIdx = name.lastIndexOf(Dash); dashIdx != -1) { // "Clash for Windows - 1 个运行窗口"
            // 零堆内存分配极速解析数字
            windows = QStringView(name).sliced(dashIdx + Dash.size()).toInt();
            name.truncate(dashIdx);
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
    AppUtil::prefetchStartAppsAsync(); // 异步后台预热外壳开始菜单映射，消除主线程冷启动阻塞

    // 原生钩子线程的 rawWheelEvent → GUI 线程的 processWheelEvent（跨线程必须 QueuedConnection）
    connect(this, &TaskbarWheelHooker::rawWheelEvent, this, &TaskbarWheelHooker::processWheelEvent,
            Qt::QueuedConnection);

    // 在专属消息线程中安装低级鼠标钩子：确保鼠标全局事件完全与 GUI 线程解耦
    std::promise<bool> installedPromise;
    auto installedFuture = installedPromise.get_future();

    m_thread = std::thread([this, &installedPromise]() {
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        m_threadId = GetCurrentThreadId();

        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        HHOOK h_mouse = SetWindowsHookExW(WH_MOUSE_LL, (HOOKPROC) mouseProc, GetModuleHandleW(nullptr), 0);
        if (!h_mouse) {
            installedPromise.set_value(false);
            return;
        }

        installedPromise.set_value(true);

        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(h_mouse);
    });

    if (installedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready && installedFuture.get()) {
        qInfo() << "TaskbarWheelHooker installed in dedicated thread ID:" << m_threadId;
    } else {
        qCritical() << "Failed to install taskbar mouse hook in dedicated thread!";
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
    if (m_threadId) {
        PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    TaskbarWheelHooker::instance = nullptr;
    UIAutomation::cleanup();
    qDebug() << "MouseHooker uninstalled";
}
