#ifndef WIN_SWITCHER_UTIL_H
#define WIN_SWITCHER_UTIL_H

#include <Windows.h>
#include <QString>
#include <QHash>
#include <QIcon>
#include <QPainterPath>
#include <dwmapi.h>
#include <QElapsedTimer>

namespace Util {
    QString getWindowTitle(HWND hwnd);
    QString getClassName(HWND hwnd);
    bool isWindowElevated(HWND hwnd);
    /// @param knownClassName 已知的窗口类名（枚举等场景已取得时传入，省去一次 GetClassName 系统调用）
    QString getWindowProcessPath(HWND hwnd, const QString& knownClassName = {});
    QList<QString> getChildProcessPaths(const QString& exePath);
    QString getFileDescription(const QString& path);
    bool isTopMost(HWND hwnd);
    void switchToWindow(HWND hwnd, bool force = false);
    void bringWindowToTop(HWND hwnd, HWND hWndInsertAfter = HWND_TOPMOST);
    /// @param pathOut 非空时输出窗口进程路径（枚举等场景已解析过时，调用方免去重复查询）
    /// @param classOut 非空时输出窗口类名（枚举时已取得，调用方免去再次查询）
    bool isWindowAcceptable(HWND hwnd, bool skipVisibleCheck = false, QString* pathOut = nullptr, QString* classOut = nullptr);
    /// @param pathMap 非空时填充 [hwnd] = 进程路径；@param classMap 非空时填充 [hwnd] = 类名（枚举时顺带取得）
    QList<HWND> enumWindows(QHash<HWND, QString>* pathMap = nullptr, QHash<HWND, QString>* classMap = nullptr);
    QList<HWND> enumChildWindows(HWND hwnd);
    /// @param pathMap 非空时填充 [hwnd] = 进程路径；@param classMap 非空时填充 [hwnd] = 类名（同上）
    QList<HWND> listValidWindows(QHash<HWND, QString>* pathMap = nullptr, QHash<HWND, QString>* classMap = nullptr);
    QList<HWND> listValidWindows(const QString& exePath);
    QList<HWND> findTopWindows(const QString& className, const QString& title = QString());
    QIcon getJumboIcon(const QString& filePath);
    QIcon getCachedIcon(const QString& path, HWND hwnd);
    void startIconPrefetch(); // 后台图标预取：工作线程提前提取图标，避免弹窗时首次同步等待
    /// 布防“双击屏蔽”：msecs 内吞掉与当前光标位置（物理坐标，±6px）同点的下一次鼠标左键按下/释放。
    /// 用于“单击即切换”后，废掉双击的第二下，避免其落到切换后的窗口上造成误触
    void armClickShield(int msecs);
    /// 解除“双击屏蔽”（弹窗每次打开时调用：新一轮交互的点击不得被上一轮的屏蔽误吞）
    void disarmClickShield();
    QPixmap getWindowIcon(HWND hwnd);
    bool setWindowRoundCorner(HWND hwnd, DWM_WINDOW_CORNER_PREFERENCE pvAttribute = DWMWCP_ROUND);
    bool isKeyPressed(int vkey);
    QIcon overlayIcon(const QPixmap& icon, const QPixmap& overlay, const QRect& overlayRect);
    HWND topWindowFromPoint(const POINT& pos);
    POINT getCursorPos();
    HWND getCurrentTaskListThumbnailWnd();
    bool isTaskbarWindow(HWND hwnd);
    /// 窗口是否被其他窗口完全遮挡（九宫格采样，用 DWM 可见边界，避开不可见边框）。
    /// 仅作启发式判断：用于"是否提示被遮挡 / 是否需要唤醒"，不追求像素级精确。
    bool isWindowFullyCovered(HWND hwnd);
    /// 该窗口所属应用是否"被遮挡就停止出帧"（Chromium / Gecko 系）。
    /// Explorer、Office、记事本等即使被完全盖住，DWM 缩略图仍然实时——不该给它们遮挡提示。
    /// @param knownExePath 调用方已解析过的进程路径（可空，空则仅按窗口类名判断）
    bool isWindowOcclusionSensitive(HWND hwnd, const QString& knownExePath = {});
    /// 超椭圆（squircle）路径：连续曲率圆角，macOS 风格方圆角（用于选中底绘制）
    QPainterPath squirclePath(const QRectF& rect, qreal exponent = 5.0);
    /// 唤醒目标窗口渲染管道（针对 Chrome/Edge/视频播放/全屏等 DirectComposition/MPO 场景，防后台冻结与黑屏）
    void nudgeWindowRendering(HWND hwnd);
} // Util

#endif //WIN_SWITCHER_UTIL_H
