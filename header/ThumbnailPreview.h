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
class QShowEvent;
class QHideEvent;

/// DWM 实时缩略图宿主组件（负责 DWM 缩略图注册、宽高比计算、高 DPI 适配与渲染）
class ThumbnailHostWidget : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailHostWidget(QWidget* parent = nullptr);
    ~ThumbnailHostWidget() override;

    void setTargetWindow(HWND hwnd);
    HWND targetWindow() const { return m_srcHwnd; }
    void updateThumbnail();
    void releaseThumbnail();

    /// 查询源窗口原生尺寸（用于外部卡片自适应宽高比）
    QSize querySourceSize() const;

    void setPadding(int padding) { m_padding = QMargins(padding, padding, padding, padding); }
    void setPadding(const QMargins& m) { m_padding = m; }
    QMargins padding() const { return m_padding; }

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    HWND m_srcHwnd = nullptr;
    HTHUMBNAIL m_hThumbnail = nullptr;
    QMargins m_padding{8, 8, 8, 8};
};

/// 方案 1：macOS 26 专属悬浮微型视窗（Floating Preview Island）
/// 具备深石墨亚克力毛玻璃、超椭圆方圆角、微光外轮廓、居中等比自适应缩略图与平滑跟随动画
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

    HWND hWnd() const { return (HWND)winId(); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void updatePopupGeometry(const QRect& iconGlobalRect, QScreen* screen);

private:
    ThumbnailHostWidget* m_host = nullptr;
    QPropertyAnimation* m_posAnim = nullptr;
    HWND m_targetHwnd = nullptr;
    QRect m_lastIconGlobalRect;
    QScreen* m_targetScreen = nullptr;
};

#endif // WIN_SWITCHER_THUMBNAILPREVIEW_H
