#include "ThumbnailHintOverlay.h"

#include <QApplication>
#include <QEasingCurve>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

namespace {
    constexpr int kHeight = 26;
    constexpr int kPadLeft = 10;
    constexpr int kPadRight = 12;
    constexpr int kGlyph = 13;
    constexpr int kGap = 6;
    const QString kText = QStringLiteral("视频窗口被遮挡，待激活后自动播放");

    QFont captionFont() {
        QFont f = QApplication::font();
        f.setPixelSize(11); // 逻辑像素：高 DPI 下由 Qt 按 devicePixelRatio 放大
        f.setWeight(QFont::Medium);
        return f;
    }
}

ThumbnailHintOverlay::ThumbnailHintOverlay(QWidget* owner)
    : QWidget(owner, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint
                     | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus
                     | Qt::WindowTransparentForInput) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    setWindowTitle(QStringLiteral("SwitchTask Hint"));
    setFont(captionFont());

    m_fade = new QPropertyAnimation(this, "windowOpacity", this);
    m_fade->setDuration(120);
    // 只在透明度归零时隐藏：淡入结束（opacity == 1）不能触发隐藏
    connect(m_fade, &QPropertyAnimation::finished, this, [this] {
        if (qFuzzyIsNull(windowOpacity())) {
            hide();
            m_fadingOut = false;
        }
    });
}

QSize ThumbnailHintOverlay::desiredSize() const {
    const QFontMetrics fm(captionFont());
    return {kPadLeft + kGlyph + kGap + fm.horizontalAdvance(kText) + kPadRight, kHeight};
}

void ThumbnailHintOverlay::showAt(const QPoint& globalTopLeft, bool dark) {
    m_dark = dark;
    setGeometry(QRect(globalTopLeft, desiredSize()));

    if (!isVisible()) {
        setWindowOpacity(0.0);
        show(); // WA_ShowWithoutActivating：不抢前台
        m_fadingOut = false;
        m_fade->stop();
        m_fade->setStartValue(0.0);
        m_fade->setEndValue(1.0);
        m_fade->setEasingCurve(QEasingCurve::OutCubic);
        m_fade->start();
    } else {
        m_fadingOut = false;
        m_fade->stop();
        setWindowOpacity(1.0);
    }

    raise(); // 压回面板之上：面板是 topmost，浮层必须同为 topmost 且在它前面
    update();
}

void ThumbnailHintOverlay::dismiss() {
    if (!isVisible() || m_fadingOut)
        return; // 已在淡出：重复调用会把起点重置为当前透明度，永远淡不完
    m_fadingOut = true;
    m_fade->stop();
    m_fade->setStartValue(windowOpacity());
    m_fade->setEndValue(0.0);
    m_fade->setEasingCurve(QEasingCurve::InCubic);
    m_fade->start();
}

void ThumbnailHintOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal radius = height() / 2.0;
    QPainterPath capsule;
    capsule.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    // 底色：比面板更高的 alpha，保证压在视频画面上依然可读
    p.setPen(Qt::NoPen);
    p.setBrush(m_dark ? QColor(28, 28, 32, 198) : QColor(250, 250, 252, 214));
    p.drawPath(capsule);

    // 顶部受光缘：与切换器面板、托盘卡片同一套光照语言
    {
        QPainterPath sheen;
        sheen.addRoundedRect(QRectF(rect()).adjusted(1.2, 1.2, -1.2, -1.2), radius - 1.2, radius - 1.2);
        QLinearGradient g(0, 1, 0, 5);
        g.setColorAt(0.0, m_dark ? QColor(255, 255, 255, 34) : QColor(255, 255, 255, 210));
        g.setColorAt(1.0, Qt::transparent);
        p.setPen(QPen(QBrush(g), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(sheen);
    }

    p.setPen(QPen(m_dark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 26), 1));
    p.setBrush(Qt::NoBrush);
    p.drawPath(capsule);

    const QColor fg = m_dark ? QColor(255, 255, 255, 235) : QColor(28, 28, 30, 235);

    // 13px 矢量"屏幕 + 斜杠"：不用 emoji，跨字体渲染会脏
    const QRectF screen(kPadLeft, (height() - (kGlyph - 3)) / 2.0, kGlyph, kGlyph - 3);
    p.setPen(QPen(fg, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(screen, 2.0, 2.0);
    p.drawLine(QPointF(screen.left() + 0.5, screen.bottom() - 0.5),
               QPointF(screen.right() - 0.5, screen.top() + 0.5));

    p.setFont(captionFont());
    p.setPen(fg);
    const int textX = kPadLeft + kGlyph + kGap;
    p.drawText(QRect(textX, 0, width() - textX - kPadRight, height()),
               Qt::AlignVCenter | Qt::AlignLeft, kText);
}
