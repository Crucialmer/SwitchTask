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
class QMouseEvent;
class QEnterEvent;
class QVariantAnimation;

/// 方案 1：macOS 26 Liquid Glass 悬浮微型卡片（Floating Preview Island）
/// 直接作为 Top-Level 顶层无边框窗口注册 DWM 缩略图（杜绝 WS_CHILD 导致的 E_INVALIDARG 失败）
class ThumbnailPopupWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QRect geometry READ geometry WRITE setGeometry)

signals:
    void windowActivated(HWND hwnd);
    void windowCloseRequested(HWND hwnd);

public:
    explicit ThumbnailPopupWidget(QWidget* parent = nullptr);
    ~ThumbnailPopupWidget() override;

    /// 同步深浅色主题与系统透明偏好（paintEvent 零 QSettings 读取）
    void applyTheme();

    /// 为指定窗口在指定图标上方呈现悬浮缩略图卡片
    void showForWindow(HWND targetHwnd, const QRect& iconGlobalRect, QScreen* screen);

    /// 平滑跟随/更新当前窗口缩略图
    void setTargetWindow(HWND targetHwnd);

    /// 关闭/隐藏并释放缩略图
    void hidePopup();

    void releaseThumbnail();

    HWND hWnd() const { return (HWND)winId(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setHovered(bool hovered);
    void updateThumbnailProperties();
    void updatePopupGeometry(const QRect& iconGlobalRect, QScreen* screen);
    QSize querySourceSize() const;

private:
    HWND m_targetHwnd = nullptr;
    HTHUMBNAIL m_hThumbnail = nullptr;
    QPropertyAnimation* m_posAnim = nullptr;
    QVariantAnimation* m_hoverAnim = nullptr;
    QRect m_lastIconGlobalRect;
    QScreen* m_targetScreen = nullptr;
    QRect m_lastFitRect;
    bool m_isDark = true;
    bool m_transparent = true;
    bool m_hovered = false;
    qreal m_hoverProgress = 0.0;
};

#endif // WIN_SWITCHER_THUMBNAILPREVIEW_H
