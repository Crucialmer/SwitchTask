#ifndef WIN_SWITCHER_THUMBNAILPREVIEW_H
#define WIN_SWITCHER_THUMBNAILPREVIEW_H

#include <QWidget>
#include <Windows.h>
#include <dwmapi.h>
#include <QRect>
#include <QSize>
#include <QScreen>
#include <QPropertyAnimation>

class QPaintEvent;
class QResizeEvent;
class QHideEvent;

/// 方案 1：macOS 26 专属悬浮微型卡片（Floating Preview Island）
/// 直接作为 Top-Level 顶层无边框窗口注册 DWM 缩略图（杜绝 WS_CHILD 导致的 E_INVALIDARG 失败）
class ThumbnailPopupWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry)

public:
    explicit ThumbnailPopupWidget(QWidget* parent = nullptr);
    ~ThumbnailPopupWidget() override;

    /// 为指定窗口在指定图标上方呈现悬浮缩略图卡片
    void showForWindow(HWND targetHwnd, const QRect& iconGlobalRect, QScreen* screen);

    /// 平滑跟随/更新当前窗口缩略图
    void setTargetWindow(HWND targetHwnd);

    /// 关闭/隐藏并释放缩略图
    void hidePopup();

    void releaseThumbnail();

    /// 实时心跳刷新（用于视频与全屏 DirectComposition 流畅连续回放）
    void heartbeatRefresh();

    HWND hWnd() const { return (HWND)winId(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateThumbnailProperties();
    void updatePopupGeometry(const QRect& iconGlobalRect, QScreen* screen);
    QSize querySourceSize() const;

private:
    HWND m_targetHwnd = nullptr;
    HTHUMBNAIL m_hThumbnail = nullptr;
    QPropertyAnimation* m_posAnim = nullptr;
    QRect m_lastIconGlobalRect;
    QScreen* m_targetScreen = nullptr;
    QRect m_lastFitRect;
};

#endif // WIN_SWITCHER_THUMBNAILPREVIEW_H
