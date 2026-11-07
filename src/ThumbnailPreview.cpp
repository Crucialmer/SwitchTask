#include "ThumbnailPreview.h"
#include "utils/Util.h"
#include "utils/Theme.h"
#include "utils/setWindowBlur.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QHideEvent>
#include <QEasingCurve>
#include <QApplication>
#include <QDebug>
#include <QTimer>

// ============================================================================
// ThumbnailPopupWidget: 方案 1 悬浮微型卡片（Floating Preview Island）
// ============================================================================

ThumbnailPopupWidget::ThumbnailPopupWidget(QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle("SwitchTask Preview");

    HWND hwnd = (HWND)winId();
    if (hwnd) {
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        Util::setWindowRoundCorner(hwnd);
        setWindowBlur(hwnd);
    }

    m_posAnim = new QPropertyAnimation(this, "geometry", this);
    m_posAnim->setDuration(110);
    m_posAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_posAnim, &QPropertyAnimation::valueChanged, this, [this](const QVariant&) {
        updateThumbnailProperties();
    });
}

ThumbnailPopupWidget::~ThumbnailPopupWidget() {
    releaseThumbnail();
}

void ThumbnailPopupWidget::releaseThumbnail() {
    if (m_hThumbnail) {
        DwmUnregisterThumbnail(m_hThumbnail);
        m_hThumbnail = nullptr;
    }
}

QSize ThumbnailPopupWidget::querySourceSize() const {
    if (!m_targetHwnd || !IsWindow(m_targetHwnd)) return {};

    if (m_hThumbnail) {
        SIZE srcSize = {0, 0};
        if (SUCCEEDED(DwmQueryThumbnailSourceSize(m_hThumbnail, &srcSize)) && srcSize.cx > 0 && srcSize.cy > 0) {
            return {static_cast<int>(srcSize.cx), static_cast<int>(srcSize.cy)};
        }
    }

    RECT rc;
    if (GetWindowRect(m_targetHwnd, &rc)) {
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w > 0 && h > 0) return {w, h};
    }
    return {};
}

void ThumbnailPopupWidget::showForWindow(HWND targetHwnd, const QRect& iconGlobalRect, QScreen* screen) {
    if (!targetHwnd || !IsWindow(targetHwnd)) {
        hidePopup();
        return;
    }

    m_lastIconGlobalRect = iconGlobalRect;
    m_targetScreen = screen ? screen : QGuiApplication::primaryScreen();

    const bool windowChanged = (m_targetHwnd != targetHwnd);
    if (windowChanged) {
        releaseThumbnail();
        m_targetHwnd = targetHwnd;
    }

    updatePopupGeometry(iconGlobalRect, m_targetScreen);

    if (!isVisible()) {
        show();
        HWND hwnd = (HWND)winId();
        if (hwnd) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        }
    }

    // 必须在窗口具有真实 HWND 后注册 DWM 缩略图（destHwnd 必须为 Top-Level 窗口）
    if (!m_hThumbnail && m_targetHwnd && IsWindow(m_targetHwnd)) {
        HRESULT hr = DwmRegisterThumbnail(hWnd(), m_targetHwnd, &m_hThumbnail);
        qInfo() << "Popup DwmRegisterThumbnail:" << Qt::hex << hr << "dest:" << hWnd() << "src:" << m_targetHwnd;
        Util::nudgeWindowRendering(m_targetHwnd);
        // 50ms 与 120ms 两次微脉冲刷新：等待后台 Chromium 收到 un-occluded 信号并完成首帧解码输出
        QTimer::singleShot(50, this, [this, hwnd = m_targetHwnd] {
            if (m_targetHwnd == hwnd && m_hThumbnail) {
                Util::nudgeWindowRendering(hwnd);
                updateThumbnailProperties();
            }
        });
        QTimer::singleShot(120, this, [this, hwnd = m_targetHwnd] {
            if (m_targetHwnd == hwnd && m_hThumbnail) {
                Util::nudgeWindowRendering(hwnd);
                updateThumbnailProperties();
            }
        });
    }

    updateThumbnailProperties();
    update();
}

void ThumbnailPopupWidget::setTargetWindow(HWND targetHwnd) {
    if (m_targetHwnd == targetHwnd) return;
    releaseThumbnail();
    m_targetHwnd = targetHwnd;

    if (isVisible() && m_targetScreen) {
        updatePopupGeometry(m_lastIconGlobalRect, m_targetScreen);
        if (m_targetHwnd && IsWindow(m_targetHwnd)) {
            HRESULT hr = DwmRegisterThumbnail(hWnd(), m_targetHwnd, &m_hThumbnail);
            qInfo() << "Popup DwmRegisterThumbnail:" << Qt::hex << hr << "dest:" << hWnd() << "src:" << m_targetHwnd;
            Util::nudgeWindowRendering(m_targetHwnd);
            QTimer::singleShot(50, this, [this, hwnd = m_targetHwnd] {
                if (m_targetHwnd == hwnd && m_hThumbnail) {
                    Util::nudgeWindowRendering(hwnd);
                    updateThumbnailProperties();
                }
            });
            QTimer::singleShot(120, this, [this, hwnd = m_targetHwnd] {
                if (m_targetHwnd == hwnd && m_hThumbnail) {
                    Util::nudgeWindowRendering(hwnd);
                    updateThumbnailProperties();
                }
            });
            updateThumbnailProperties();
        }
        update();
    }
}

void ThumbnailPopupWidget::hidePopup() {
    if (m_posAnim) m_posAnim->stop();
    hide();
    releaseThumbnail();
    m_targetHwnd = nullptr;
}

void ThumbnailPopupWidget::updateThumbnailProperties() {
    if (!m_hThumbnail || !m_targetHwnd || !IsWindow(m_targetHwnd) || !isVisible()) {
        return;
    }

    SIZE srcSize = {0, 0};
    if (FAILED(DwmQueryThumbnailSourceSize(m_hThumbnail, &srcSize)) || srcSize.cx <= 0 || srcSize.cy <= 0) {
        RECT rc;
        if (GetWindowRect(m_targetHwnd, &rc)) {
            srcSize.cx = rc.right - rc.left;
            srcSize.cy = rc.bottom - rc.top;
        }
    }

    // 四周留出 8px macOS 微光边框
    QRect contentRect = rect().adjusted(8, 8, -8, -8);
    if (contentRect.width() <= 0 || contentRect.height() <= 0) return;

    QRect fitRect = contentRect;
    if (srcSize.cx > 0 && srcSize.cy > 0) {
        double aspect = static_cast<double>(srcSize.cx) / static_cast<double>(srcSize.cy);
        int targetW = contentRect.width();
        int targetH = qRound(targetW / aspect);
        if (targetH > contentRect.height()) {
            targetH = contentRect.height();
            targetW = qRound(targetH * aspect);
        }
        int x = contentRect.left() + (contentRect.width() - targetW) / 2;
        int y = contentRect.top() + (contentRect.height() - targetH) / 2;
        fitRect = QRect(x, y, targetW, targetH);
    }
    m_lastFitRect = fitRect;

    // Per-Monitor V2 物理像素坐标映射
    const qreal dpr = devicePixelRatioF();
    RECT rcDest;
    rcDest.left   = qRound(fitRect.left() * dpr);
    rcDest.top    = qRound(fitRect.top() * dpr);
    rcDest.right  = qRound(fitRect.right() * dpr);
    rcDest.bottom = qRound(fitRect.bottom() * dpr);

    if (rcDest.right <= rcDest.left || rcDest.bottom <= rcDest.top) return;

    DWM_THUMBNAIL_PROPERTIES props = {};
    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    props.rcDestination = rcDest;
    props.opacity = 255;
    props.fVisible = TRUE;
    if (srcSize.cx > 0 && srcSize.cy > 0) {
        props.dwFlags |= DWM_TNP_RECTSOURCE;
        props.rcSource = RECT{0, 0, srcSize.cx, srcSize.cy};
    }

    DwmUpdateThumbnailProperties(m_hThumbnail, &props);
}

void ThumbnailPopupWidget::updatePopupGeometry(const QRect& iconGlobalRect, QScreen* screen) {
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    constexpr int kMaxW = 340;
    constexpr int kMaxH = 210;
    constexpr int kMinW = 200;
    constexpr int kMinH = 130;
    constexpr int kPadding = 16;

    QSize srcSize = querySourceSize();
    int cardW = kMaxW;
    int cardH = kMaxH;

    if (srcSize.width() > 0 && srcSize.height() > 0) {
        double aspect = static_cast<double>(srcSize.width()) / static_cast<double>(srcSize.height());
        int availW = kMaxW - kPadding;
        int availH = kMaxH - kPadding;
        int fitW = availW;
        int fitH = qRound(fitW / aspect);
        if (fitH > availH) {
            fitH = availH;
            fitW = qRound(fitH * aspect);
        }
        cardW = qBound(kMinW, fitW + kPadding, kMaxW);
        cardH = qBound(kMinH, fitH + kPadding, kMaxH);
    }

    const int iconCenterX = iconGlobalRect.center().x();
    int x = iconCenterX - cardW / 2;
    int y = iconGlobalRect.top() - cardH - 12;

    QRect avail = screen->availableGeometry();
    if (x < avail.left() + 8) x = avail.left() + 8;
    if (x + cardW > avail.right() - 8) x = avail.right() - 8 - cardW;
    if (y < avail.top() + 8) {
        y = iconGlobalRect.bottom() + 12;
    }

    QRect targetGeo(x, y, cardW, cardH);

    if (isVisible()) {
        m_posAnim->stop();
        m_posAnim->setStartValue(geometry());
        m_posAnim->setEndValue(targetGeo);
        m_posAnim->start();
    } else {
        setGeometry(targetGeo);
    }
}

void ThumbnailPopupWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateThumbnailProperties();
}

void ThumbnailPopupWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    releaseThumbnail();
}

void ThumbnailPopupWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // macOS 风格：连续圆角（r=14）面板底色
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::panelFill());
    painter.drawPath(panel);

    // 顶部受光缘
    {
        QPainterPath inner;
        inner.addRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 13, 13);
        QLinearGradient edge(0, 0, 0, 4);
        edge.setColorAt(0.0, Theme::panelTopSheen());
        edge.setColorAt(1.0, Qt::transparent);
        painter.setPen(QPen(QBrush(edge), 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(inner);
    }

    // 细微外轮廓描边
    painter.setPen(QPen(Theme::panelStroke(), 1));
    painter.drawPath(panel);

    // 若缩略图尚未就绪（如窗口不可见或最小化），优雅居中提示
    if (!m_hThumbnail || !m_targetHwnd || !IsWindow(m_targetHwnd)) {
        painter.setPen(QColor(255, 255, 255, 120));
        QFont f = painter.font();
        f.setPointSize(9);
        painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("窗口已最小化或暂无画面"));
    }
}
