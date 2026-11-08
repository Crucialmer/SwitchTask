#include "utils/KeyboardHooker.h"
#include <QDebug>
#include <QApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <future>
#include "utils/Util.h"
#include "widget.h"

// 钩子回调日志类别：默认级别全开（默认输出与原先逐字一致）；现场可用
// QT_LOGGING_RULES="switchtask.hook.debug=false" 精准静音（勿默认禁用——保留既有调试输出）
Q_LOGGING_CATEGORY(lcHook, "switchtask.hook")

namespace {
    // 修饰键内部状态跟踪（消除 GetAsyncKeyState 系统调用与状态滞后，符合 MSDN 低级键盘钩子规范）
    static bool s_altDown = false;
    static bool s_ctrlDown = false;
    static bool s_shiftDown = false;
    static bool s_winDown = false;

    // Tab 自动重复守卫：按住按键时系统约30ms触发一次重复 keydown，pinned 模式下重复的
    // requestShowPinned 会被误判为"再次按下=确认切换"，导致弹出窗口刚显示就被切走（无法保持）。
    // 仅对 Ctrl+Alt+Tab 呼出去重；普通 Alt+Tab 的重复仍放行（按住 Tab 连续切换是原设计行为）。
    static bool s_ctrlTabDown = false;

    // Ctrl+W 自动重复守卫：按住 Ctrl+W 时系统会连续触发重复 keydown；此时若每个重复都执行关闭，
    // 会在"条目被移除后自动改选"的语义下连续关闭用户并未选中的窗口——按住只关一个，松开再按关下一个。
    static bool s_wDown = false;
    static ULONGLONG s_wDownAt = 0; // 守卫激活时刻（超时自愈用）
}

LRESULT CALLBACK keyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    using Hooker = KeyboardHooker;
    if (nCode == HC_ACTION) {
        auto* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (wParam == WM_SYSKEYDOWN || wParam == WM_KEYDOWN) { // Alt & [Alt按下时的Tab]属于SysKey
            // 跟踪修饰键按下状态（零系统调用）
            if (pKeyBoard->vkCode == VK_LMENU || pKeyBoard->vkCode == VK_RMENU || pKeyBoard->vkCode == VK_MENU)
                s_altDown = true;
            else if (pKeyBoard->vkCode == VK_LCONTROL || pKeyBoard->vkCode == VK_RCONTROL || pKeyBoard->vkCode == VK_CONTROL)
                s_ctrlDown = true;
            else if (pKeyBoard->vkCode == VK_LSHIFT || pKeyBoard->vkCode == VK_RSHIFT || pKeyBoard->vkCode == VK_SHIFT)
                s_shiftDown = true;
            else if (pKeyBoard->vkCode == VK_LWIN || pKeyBoard->vkCode == VK_RWIN)
                s_winDown = true;

            // Ctrl+W 按住期间的重复 keydown：无论弹窗是否仍显示，一律吞掉直到 keyup——
            // 否则"关掉最后一项、弹窗自动收起"后残留的重复会漏给恢复焦点的窗口，触发对方的 Ctrl+W
            if (pKeyBoard->vkCode == 'W' && s_wDown) {
                // 超时自愈：keyup 丢失（钩子被系统跳过/被其他钩子吞掉）时，守卫不至于永久吞掉 W 键
                if (GetTickCount64() - s_wDownAt < 2000)
                    return 1;
                s_wDown = false;
            }

            // Alt 状态：由硬件标志 LLKHF_ALTDOWN 或内部状态判断，零系统调用
            const bool isAltPressed = ((pKeyBoard->flags & LLKHF_ALTDOWN) != 0) || s_altDown;
            auto* targetWidget = Hooker::receiver.data();

            if (isAltPressed && targetWidget) {
                if (pKeyBoard->vkCode == VK_TAB) {
                    const bool isCtrlPressed = s_ctrlDown;
                    if (isCtrlPressed) {
                        if (s_ctrlTabDown)
                            return 1; // 自动重复事件，忽略（只处理首次按下）
                        s_ctrlTabDown = true;
                        // Ctrl+Alt+Tab：松手后切换器保持显示（pinned），Enter/点击确认，Esc 取消
                        qCDebug(lcHook) << "Ctrl+Alt+Tab detected!";
                        QMetaObject::invokeMethod(targetWidget, "requestShowPinned", Qt::QueuedConnection);
                        return 1; // 阻止事件传递（拦截系统的 Ctrl+Alt+Tab 固定切换器）
                    }
                    qCDebug(lcHook) << "Alt+Tab detected!";
                    const bool isPopupActive = Hooker::s_receiverHwnd && IsWindowVisible(Hooker::s_receiverHwnd) && !IsIconic(Hooker::s_receiverHwnd);
                    if (isPopupActive) {
                        // 弹出器已显示（pinned）：直接转发循环选择——不依赖键盘焦点，
                        // 抢焦点失败（可见但非前台）时也能连续切换，且不刷新列表、不丢选择
                        auto shiftModifier = s_shiftDown ? Qt::ShiftModifier : Qt::NoModifier;
                        QApplication::postEvent(targetWidget, new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier | shiftModifier));
                    } else if (Hooker::s_receiverHwnd != GetForegroundWindow()) { // not Foreground
                        // 异步，防止阻塞；超过1s会导致被系统强制绕过，传递给下一个钩子
                        // 方案A：Alt+Tab 也走 pinned 模式——松开 Alt 后切换器保持显示，Enter/空格/点击确认，Esc 取消
                        QMetaObject::invokeMethod(targetWidget, "requestShowPinned", Qt::QueuedConnection);
                    } else {
                        // 转发Alt+Tab给Widget
                        auto shiftModifier = s_shiftDown ? Qt::ShiftModifier : Qt::NoModifier;
                        auto tabDownEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier | shiftModifier);
                        QApplication::postEvent(targetWidget, tabDownEvent); // async
                    }
                    return 1; // 阻止事件传递
                } else if (pKeyBoard->vkCode == VK_OEM_3) { // ~`
                    qCDebug(lcHook) << "Alt+` detected!";
                    auto shiftModifier = s_shiftDown ? Qt::ShiftModifier : Qt::NoModifier;
                    auto event = new QKeyEvent(QEvent::KeyPress, Qt::Key_QuoteLeft, Qt::AltModifier | shiftModifier);
                    QApplication::postEvent(targetWidget, event); // async
                    return 1; // 阻止事件传递
                }
            }

            // 弹出器显示中（pinned）：接管选择/确认/取消键——不依赖系统键盘焦点。
            const HWND fgHwnd = GetForegroundWindow();
            const bool isForeground = (Hooker::s_receiverHwnd && fgHwnd == Hooker::s_receiverHwnd);
            const bool popupActive = Hooker::s_receiverHwnd && IsWindowVisible(Hooker::s_receiverHwnd) && !IsIconic(Hooker::s_receiverHwnd);
            if (popupActive && targetWidget) {
                const bool ctrl = s_ctrlDown;
                const bool winPressed = s_winDown;
                int qtKey = 0;
                if (!winPressed && isForeground) {
                    switch (pKeyBoard->vkCode) {
                        // Alt 按住时 Tab/` 已由上方分支处理
                        case VK_TAB:    if (!isAltPressed && !ctrl) qtKey = Qt::Key_Tab; break;
                        case VK_OEM_3:  if (!isAltPressed && !ctrl) qtKey = Qt::Key_QuoteLeft; break;
                        case VK_RETURN: if (!ctrl && isForeground) qtKey = Qt::Key_Return; break; // 确认键有副作用：仅弹窗持有前台时接管（防止吞掉其他程序里的回车）
                        case VK_SPACE:  if (!ctrl && isForeground) qtKey = Qt::Key_Space; break;
                        case VK_ESCAPE: if (!ctrl) qtKey = Qt::Key_Escape; break; // Ctrl+Esc=开始菜单，放行
                        case VK_UP:     if (!ctrl) qtKey = Qt::Key_Up; break;
                        case VK_DOWN:   if (!ctrl) qtKey = Qt::Key_Down; break;
                        case VK_LEFT:   if (!ctrl) qtKey = Qt::Key_Left; break;
                        case VK_RIGHT:  if (!ctrl) qtKey = Qt::Key_Right; break;
                        case 'H':      if (isForeground && !ctrl) qtKey = Qt::Key_Left; break;
                        case 'J':      if (isForeground && !ctrl) qtKey = Qt::Key_Down; break;
                        case 'K':      if (isForeground && !ctrl) qtKey = Qt::Key_Up; break;
                        case 'L':      if (isForeground && !ctrl) qtKey = Qt::Key_Right; break;
                        case 'W':
                            if (isForeground && ctrl && !s_shiftDown) { // Ctrl+Shift+W 是许多软件的快捷键：放行，勿吞
                                s_wDown = true;
                                s_wDownAt = GetTickCount64();
                                qtKey = Qt::Key_W;
                            }
                            break;
                        default: break;
                    }
                }
                if (qtKey) {
                    Qt::KeyboardModifiers mods = s_shiftDown ? Qt::ShiftModifier : Qt::NoModifier;
                    if (qtKey == Qt::Key_Tab)
                        mods |= Qt::AltModifier; // 无修饰键的 Tab 会被 QWidget::event 当焦点切换吞掉，带 Alt 可绕过
                    if (qtKey == Qt::Key_W)
                        mods |= Qt::ControlModifier; // 其余映射键都要求 !ctrl，带 Control 的只可能是 W
                    QApplication::postEvent(targetWidget, new QKeyEvent(QEvent::KeyPress, qtKey, mods));
                    return 1; // 阻止系统投递：按键仅由弹出器处理
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            auto* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

            // 跟踪修饰键释放状态（零系统调用）
            if (pKeyBoard->vkCode == VK_LMENU || pKeyBoard->vkCode == VK_RMENU || pKeyBoard->vkCode == VK_MENU)
                s_altDown = false;
            else if (pKeyBoard->vkCode == VK_LCONTROL || pKeyBoard->vkCode == VK_RCONTROL || pKeyBoard->vkCode == VK_CONTROL)
                s_ctrlDown = false;
            else if (pKeyBoard->vkCode == VK_LSHIFT || pKeyBoard->vkCode == VK_RSHIFT || pKeyBoard->vkCode == VK_SHIFT)
                s_shiftDown = false;
            else if (pKeyBoard->vkCode == VK_LWIN || pKeyBoard->vkCode == VK_RWIN)
                s_winDown = false;

            if (pKeyBoard->vkCode == VK_TAB)
                s_ctrlTabDown = false; // 释放时重置自动重复守卫
            if (pKeyBoard->vkCode == 'W')
                s_wDown = false; // Ctrl+W 释放：重置自动重复守卫

            auto* targetWidget = Hooker::receiver.data();
            if ((pKeyBoard->vkCode == VK_LMENU || pKeyBoard->vkCode == VK_RMENU) && targetWidget) {
                qCDebug(lcHook) << "Alt released!";
                auto event = new QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
                QApplication::postEvent(targetWidget, event); // async
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

KeyboardHooker::KeyboardHooker(QWidget* _receiver) {
    if (KeyboardHooker::receiver) {
        qWarning() << "Only one KeyboardHooker can be installed!";
        return;
    }
    if (!_receiver) {
        qWarning() << "Receiver is nullptr!";
        return;
    }
    KeyboardHooker::receiver = _receiver;
    KeyboardHooker::s_receiverHwnd = reinterpret_cast<HWND>(_receiver->winId());

    std::promise<bool> installedPromise;
    auto installedFuture = installedPromise.get_future();

    m_thread = std::thread([this, &installedPromise]() {
        // 强制创建线程消息队列
        MSG msg;
        PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        m_threadId = GetCurrentThreadId();

        // 提升钩子线程优先级为最高，保证系统按键派发零延迟
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        // 初始化修饰键初始状态（自愈初始按下）
        s_altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        s_ctrlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        s_shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        s_winDown = (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

        HHOOK h_keyboard = SetWindowsHookExW(WH_KEYBOARD_LL, (HOOKPROC) keyboardProc, GetModuleHandleW(nullptr), 0);
        if (!h_keyboard) {
            installedPromise.set_value(false);
            return;
        }

        installedPromise.set_value(true);

        // 独立专属消息循环：永远不被 GUI 主线程的绘制、计算或 I/O 阻塞
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        UnhookWindowsHookEx(h_keyboard);
    });

    if (installedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready && installedFuture.get()) {
        qInfo() << "KeyboardHooker installed in dedicated thread ID:" << m_threadId;
    } else {
        qWarning() << "Failed to install KeyboardHooker in dedicated thread!";
    }
}

KeyboardHooker::~KeyboardHooker() {
    if (m_threadId) {
        PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    KeyboardHooker::receiver = nullptr;
    KeyboardHooker::s_receiverHwnd = nullptr;
    qDebug() << "KeyboardHooker uninstalled";
}
