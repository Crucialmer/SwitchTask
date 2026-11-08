#include "utils/winEventHook.h"
#include <QDebug>
#include <QTime>

static HWINEVENTHOOK s_fgHook = nullptr;
static HWINEVENTHOOK s_destroyHook = nullptr;
static WinEventCallback s_callback = nullptr;

void CALLBACK WinEventProc(HWINEVENTHOOK /*hWinEventHook*/, DWORD event, HWND hwnd, LONG idObject, LONG idChild,
                           DWORD /*dwEventThread*/, DWORD /*dwmsEventTime*/) {
    // 仅监听顶级窗口级别事件（过滤内部子控件、按钮、caret等海量事件）
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
        return;

    if (s_callback)
        s_callback(event, hwnd);
}

bool setWinEventHook(WinEventCallback _callback) {
    if (s_fgHook || s_destroyHook) {
        qWarning() << "WinEventHook already set. Unhook first.";
        return false;
    }

    s_callback = std::move(_callback);

    // 监听前台窗口切换事件
    s_fgHook = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND,
        EVENT_SYSTEM_FOREGROUND,
        nullptr,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS // 跳过自身进程的事件
    );

    // 监听顶级窗口销毁事件（实时清理活跃历史与驻留列表项）
    s_destroyHook = SetWinEventHook(
        EVENT_OBJECT_DESTROY,
        EVENT_OBJECT_DESTROY,
        nullptr,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS // 跳过自身进程的事件
    );

    qDebug() << "Set WinEventHook (Foreground + Destroy).";

    return s_fgHook != nullptr;
}

void unhookWinEvent() {
    if (!s_fgHook && !s_destroyHook) return;

    s_callback = nullptr;
    if (s_fgHook) {
        UnhookWinEvent(s_fgHook);
        s_fgHook = nullptr;
    }
    if (s_destroyHook) {
        UnhookWinEvent(s_destroyHook);
        s_destroyHook = nullptr;
    }

    qDebug() << "Unhook win event.";
}
