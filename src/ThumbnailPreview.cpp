#include "ThumbnailPreview.h"
#include "utils/Util.h"
#include "utils/Theme.h"
#include "utils/setWindowBlur.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QEasingCurve>
#include <QApplication>
#include <QVBoxLayout>

// ============================================================================
// ThumbnailHostWidget: 承载 DWM 缩略图的原生控件
// ============================================================================

ThumbnailHostWidget::ThumbnailHostWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_NativeWindow, true); // 必须拥有独立 HWND 供 DWM 目标渲染
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

ThumbnailHostWidget::~ThumbnailHostWidget() {
    releaseThumbnail();
}

void ThumbnailHostWidget::setTargetWindow(HWND hwnd) {
    if (m_srcHwnd == hwnd) {
        if (hwnd && isVisible() && !m_hThumbnail) {
            updateThumbnail();
        }
        return;
    }
    releaseThumbnail();
    m_srcHwnd = hwnd;
    if (isVisible() && m_srcHwnd) {
        updateThumbnail();
    }
    update();
}

void ThumbnailHostWidget::releaseThumbnail() {
    if (m_hThumbnail) {
        DwmUnregisterThumbnail(m_hThumbnail);
        m_hThumbnail = nullptr;
    }
}

QSize ThumbnailHostWidget::querySourceSize() const {
    if (!m_srcHwnd || !IsWindow(m_srcHwnd)) return {};

    if (m_hThumbnail) {
        SIZE srcSize = {0, 0};
        if (SUCCEEDED(DwmQueryThumbnailSourceSize(m_hThumbnail, &srcSize)) && srcSize.cx > 0 && srcSize.cy > 0) {
            return {static_cast<int>(srcSize.cx), static_cast<int>(srcSize.cy)};
        }
    }

    // 备用：从源窗口的真实尺寸获取
    RECT rc;
    if (GetWindowRect(m_srcHwnd, &rc)) {
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        if (w > 0 && h > 0) return {w, h};
    }
    return {};
}

void ThumbnailHostWidget::updateThumbnail() {
    if (!m_srcHwnd || !IsWindow(m_srcHwnd) || !isVisible()) {
        releaseThumbnail();
        return;
    }

    HWND destHwnd = (HWND)winId();
    if (!destHwnd) return;

    if (!m_hThumbnail) {
        HRESULT hr = DwmRegisterThumbnail(destHwnd, m_srcHwnd, &m_hThumbnail);
        if (FAILED(hr)) {
            m_hThumbnail = nullptr;
            return;
        }
    }

    SIZE srcSize = {0, 0};
    DwmQueryThumbnailSourceSize(m_hThumbnail, &srcSize);

    QRect contentRect = rect().marginsRemoved(m_padding);
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

    // 8K 高 DPI 物理像素换算（Per-Monitor V2 规范）
    const qreal dpr = devicePixelRatioF();
    RECT rcDest;
    rcDest.left   = qRound(fitRect.left() * dpr);
    rcDest.top    = qRound(fitRect.top() * dpr);
    rcDest.right  = qRound(fitRect.right() * dpr);
    rcDest.bottom = qRound(fitRect.bottom() * dpr);

    DWM_THUMBNAIL_PROPERTIES props = {};
    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    props.rcDestination = rcDest;
    props.opacity = 255;
    props.fVisible = TRUE;

    DwmUpdateThumbnailProperties(m_hThumbnail, &props);
}

void ThumbnailHostWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateThumbnail();
}

void ThumbnailHostWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateThumbnail();
}

void ThumbnailHostWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    releaseThumbnail();
}

void ThumbnailHostWidget::paintEvent(QPaintEvent*) {
    // 缩略图未就绪或窗口最小化时的 macOS 优雅占位
    if (!m_srcHwnd || !IsWindow(m_srcHwnd) || !m_hThumbnail) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRect contentRect = rect().marginsRemoved(m_padding);
        QPainterPath path;
        path.addRoundedRect(QRectF(contentRect), 8, 8);
        p.fillPath(path, QColor(0, 0, 0, 30));

        p.setPen(QColor(255, 255, 255, 120));
        QFont f = p.font();
        f.setPointSize(9);
        p.setFont(f);
        p.drawText(contentRect, Qt::AlignCenter, QStringLiteral("实时画面载入中..."));
    }
}

// ============================================================================
// ThumbnailPopupWidget: 方案 1 悬浮微型视窗（Floating Preview Island）
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

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(0);

    m_host = new ThumbnailHostWidget(this);
    m_host->setPadding(0);
    layout->addWidget(m_host);

    m_posAnim = new QPropertyAnimation(this, "geometry", this);
    m_posAnim->setDuration(110);
    m_posAnim->setEasingCurve(QEasingCurve::OutCubic);
}

ThumbnailPopupWidget::~ThumbnailPopupWidget() {
    hidePopup();
}

void ThumbnailPopupWidget::showForWindow(HWND targetHwnd, const QRect& iconGlobalRect, QScreen* screen) {
    if (!targetHwnd || !IsWindow(targetHwnd)) {
        hidePopup();
        return;
    }

    m_targetHwnd = targetHwnd;
    m_lastIconGlobalRect = iconGlobalRect;
    m_targetScreen = screen;

    m_host->setTargetWindow(targetHwnd);
    updatePopupGeometry(iconGlobalRect, screen);

    if (!isVisible()) {
        HWND hwnd = (HWND)winId();
        if (hwnd) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
        }
        show();
    }
}

void ThumbnailPopupWidget::setTargetWindow(HWND targetHwnd) {
    if (m_targetHwnd == targetHwnd) return;
    m_targetHwnd = targetHwnd;
    m_host->setTargetWindow(targetHwnd);
    if (isVisible() && m_targetScreen) {
        updatePopupGeometry(m_lastIconGlobalRect, m_targetScreen);
    }
}

void ThumbnailPopupWidget::hidePopup() {
    if (m_posAnim) m_posAnim->stop();
    hide();
    m_host->releaseThumbnail();
    m_targetHwnd = nullptr;
}

void ThumbnailPopupWidget::updatePopupGeometry(const QRect& iconGlobalRect, QScreen* screen) {
    if (!screen) screen = QGuiApplication::primaryScreen();
    if (!screen) return;

    // 根据源窗口尺寸计算黄金卡片宽高
    constexpr int kMaxW = 340;
    constexpr int kMaxH = 210;
    constexpr int kMinW = 200;
    constexpr int kMinH = 130;
    constexpr int kPadding = 16; // 左右/上下各 8px 边距

    QSize srcSize = m_host->querySourceSize();
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

    // 绝对居中于图标上方
    const int iconCenterX = iconGlobalRect.center().x();
    int x = iconCenterX - cardW / 2;
    int y = iconGlobalRect.top() - cardH - 12; // 留出 12px 呼吸间距

    // 屏幕边缘贴合保护（防出屏）
    QRect avail = screen->availableGeometry();
    if (x < avail.left() + 8) x = avail.left() + 8;
    if (x + cardW > avail.right() - 8) x = avail.right() - 8 - cardW;
    if (y < avail.top() + 8) {
        // 若上方空间不足，贴紧屏幕顶缘
        y = avail.top() + 8;
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

void ThumbnailPopupWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // macOS 风格：连续圆角（r=14）面板底色
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 14, 14);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Theme::panelFill());
    painter.drawPath(panel);

    // Apple 式背景材质层次：漫反射光影渐变（上亮→下沉）
    {
        QLinearGradient shade(0, 0, 0, height());
        shade.setColorAt(0.0, Theme::panelGradTop());
        shade.setColorAt(0.45, Qt::transparent);
        shade.setColorAt(0.75, Qt::transparent);
        shade.setColorAt(1.0, Theme::panelGradBottom());
        painter.setBrush(shade);
        painter.drawPath(panel);
    }

    // 顶部受光缘（内侧细微高光）
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
}

void ThumbnailPopupWidget::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (m_host) m_host->releaseThumbnail();
}
