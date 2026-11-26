#include "ThumbnailPreview.h"
#include "utils/Util.h"
#include "utils/Theme.h"
#include "utils/setWindowBlur.h"
#include <QPainter>
#include <QResizeEvent>
#include <QHideEvent>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QEasingCurve>
#include <QVariantAnimation>
#include <QApplication>
#include <QDebug>
#include <QTimer>

// ============================================================================
// ThumbnailPopupWidget: 方案 1 macOS 26 Liquid Glass 悬浮微型卡片（Floating Preview Island）
// ============================================================================

ThumbnailPopupWidget::ThumbnailPopupWidget(QWidget* parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowTitle("SwitchTask Preview");
    setCursor(Qt::PointingHandCursor);

    HWND hwnd = (HWND)winId();
    if (hwnd) {
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
        Util::setWindowRoundCorner(hwnd);
    }

    m_posAnim = new QPropertyAnimation(this, "geometry", this);
    m_posAnim->setDuration(110);
    m_posAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_posAnim, &QPropertyAnimation::valueChanged, this, [this](const QVariant&) {
        updateThumbnailProperties();
    });

    m_hoverAnim = new QVariantAnimation(this);
    m_hoverAnim->setDuration(140);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_hoverProgress = v.toReal();
        update();
    });

    applyTheme();
}

ThumbnailPopupWidget::~ThumbnailPopupWidget() {
    releaseThumbnail();
}

void ThumbnailPopupWidget::applyTheme() {
    m_isDark = Theme::isSwitcherDark();
    m_transparent = Theme::isTransparencyEnabled();
    if (HWND hwnd = hWnd()) {
        setWindowBlur(hwnd);
    }
    update();
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
    if (m_hoverAnim) m_hoverAnim->stop();
    m_hovered = false;
    m_hoverProgress = 0.0;
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

    // 10px 留白与外圆角 r=18 + 内井 r=10 同心呼应
    QRect contentRect = rect().adjusted(10, 10, -10, -10);
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

    const QRect avail = screen->availableGeometry();
    const int kMaxW = qBound(540, qRound(avail.width() * 0.35), 2400);
    const int kMaxH = qBound(340, qRound(avail.height() * 0.35), 1500);
    const int kMinW = qRound(kMaxW * 0.45);
    const int kMinH = qRound(kMaxH * 0.45);
    constexpr int kPadding = 20;

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

void ThumbnailPopupWidget::setHovered(bool hovered) {
    if (m_hovered == hovered) return;
    m_hovered = hovered;
    if (m_hoverAnim && isVisible()) {
        m_hoverAnim->stop();
        m_hoverAnim->setStartValue(m_hoverProgress);
        m_hoverAnim->setEndValue(m_hovered ? 1.0 : 0.0);
        m_hoverAnim->start();
    } else {
        m_hoverProgress = m_hovered ? 1.0 : 0.0;
        update();
    }
}

void ThumbnailPopupWidget::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    setHovered(true);
}

void ThumbnailPopupWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    setHovered(false);
}

void ThumbnailPopupWidget::mousePressEvent(QMouseEvent* event) {
    if (m_targetHwnd && IsWindow(m_targetHwnd)) {
        if (event->button() == Qt::LeftButton) {
            emit windowActivated(m_targetHwnd);
            event->accept();
            return;
        } else if (event->button() == Qt::MiddleButton) {
            emit windowCloseRequested(m_targetHwnd);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void ThumbnailPopupWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // macOS 26 Liquid Glass 外壳（r=18 连续超椭圆）+ 同心内凹光学井（r=10，18 - 8 = 10 严格同心）
    Theme::paintLiquidGlassSurface(painter, QRectF(rect()), 18.0, m_isDark, m_transparent);
    Theme::paintLiquidGlassStageWell(painter, QRectF(rect()).adjusted(8.0, 8.0, -8.0, -8.0), 10.0, m_hoverProgress, m_isDark);

    // 若缩略图尚未就绪（如窗口不可见或最小化），优雅居中提示
    if (!m_hThumbnail || !m_targetHwnd || !IsWindow(m_targetHwnd)) {
        painter.setPen(Theme::stageEmptyText(m_isDark));
        QFont f = painter.font();
        f.setPointSize(9);
        painter.setFont(f);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("窗口已最小化或暂无画面"));
    }
}
