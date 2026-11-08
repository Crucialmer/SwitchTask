#ifndef WIN_SWITCHER_APPUTIL_H
#define WIN_SWITCHER_APPUTIL_H

#include <Windows.h>
#include <QIcon>

namespace AppUtil {
    HWND getAppFrameWindow(HWND hwnd);
    HWND getAppCoreWindow(HWND hwnd);
    bool isAppFrameWindow(HWND hwnd);
    QIcon getAppIcon(const QString& path);
    QString getExePathFromAppIdOrName(const QString& appid = QString(), const QString& appName = QString());
    void prefetchStartAppsAsync(); // 异步后台预热外壳开始菜单应用列表，避免阻塞主线程冷启动

    inline const QString AppCoreWindowClass = "Windows.UI.Core.CoreWindow";
    inline const QString AppFrameWindowClass = "ApplicationFrameWindow";
    inline const QString AppManifest = "AppxManifest.xml";
}


#endif //WIN_SWITCHER_APPUTIL_H
