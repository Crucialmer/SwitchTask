#include "utils/MacToast.h"
#include "utils/Theme.h"
#include "utils/ConfigManager.h"
#include "utils/setWindowBlur.h"
#include "utils/Util.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QPainter>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include <QStyleOption>
#include <QStyle>

namespace {

/// macOS 气泡左侧应用图标 + 状态角标自绘组件
class ToastIconWidget : public QWidget {
public:
    explicit ToastIconWidget(MacToast::Type type, QWidget* parent = nullptr)
        : QWidget(parent), m_type(type) {
        setFixedSize(38, 38);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        // 绘制 32x32 应用主图标（支持 4K/8K 高 DPI Retina 锐利绘制）
        const qreal dpr = devicePixelRatioF();
        QPixmap icon = QIcon(":/img/icon.ico").pixmap(QSize(qRound(32 * dpr), qRound(32 * dpr)));
        icon.setDevicePixelRatio(dpr);
        p.drawPixmap(0, 0, icon);

        // 绘制右下角 14x14 矢量状态微标
        const QRectF badgeRect(22.5, 22.5, 14, 14);
        QColor badgeBg;
        if (m_type == MacToast::Success) {
            badgeBg = QColor(0x34, 0xC7, 0x59); // Apple Traffic Green
        } else if (m_type == MacToast::Warning) {
            badgeBg = QColor(0xFF, 0x95, 0x00); // Apple Orange
        } else {
            badgeBg = QColor(0x00, 0x7A, 0xFF); // Apple Blue
        }

        // 状态微标外圈高光反色描边
        p.setPen(QPen(Theme::isDark() ? QColor(30, 30, 35) : QColor(255, 255, 255), 1.5));
        p.setBrush(badgeBg);
        p.drawEllipse(badgeRect);

        // 徽标内部符号（抗锯齿纯白矢量）
        p.setPen(QPen(Qt::white, 1.25, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        if (m_type == MacToast::Success) {
            // 对勾 √
            p.drawLine(QPointF(26.0, 29.5), QPointF(28.5, 32.0));
            p.drawLine(QPointF(28.5, 32.0), QPointF(33.0, 27.0));
        } else if (m_type == MacToast::Warning) {
            // 感叹号 !
            p.drawLine(QPointF(29.5, 26.0), QPointF(29.5, 30.0));
            p.drawPoint(QPointF(29.5, 32.5));
        } else {
            // 信息标 i
            p.drawLine(QPointF(29.5, 28.0), QPointF(29.5, 32.0));
            p.drawPoint(QPointF(29.5, 26.0));
        }
    }

private:
    MacToast::Type m_type;
};

} // namespace

MacToast::MacToast(
    const QString& title,
    const QString& message,
    Type type,
    int durationMs,
    std::function<void()> onClick,
    QWidget* parent
) : QWidget(parent), m_durationMs(durationMs), m_onClick(std::move(onClick)) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating); // 绝对不抢夺前台活动焦点
    setCursor(Qt::PointingHandCursor);
    setFixedSize(340, 68);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_frame = new QFrame(this);
    m_frame->setObjectName("macToastFrame");

    auto* frameLayout = new QHBoxLayout(m_frame);
    frameLayout->setContentsMargins(12, 10, 14, 10);
    frameLayout->setSpacing(12);

    auto* iconWidget = new ToastIconWidget(type, m_frame);
    frameLayout->addWidget(iconWidget, 0, Qt::AlignVCenter);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(3);

    auto* topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(8);

    auto* titleLabel = new QLabel(title, m_frame);
    titleLabel->setObjectName("macToastTitle");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    topRow->addWidget(titleLabel, 0, Qt::AlignVCenter);

    topRow->addStretch();

    auto* timeLabel = new QLabel(QStringLiteral("现在"), m_frame);
    timeLabel->setObjectName("macToastTime");
    timeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    topRow->addWidget(timeLabel, 0, Qt::AlignVCenter);

    textLayout->addLayout(topRow);

    auto* msgLabel = new QLabel(message, m_frame);
    msgLabel->setObjectName("macToastMessage");
    msgLabel->setWordWrap(true);
    msgLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    textLayout->addWidget(msgLabel, 1, Qt::AlignVCenter);

    frameLayout->addLayout(textLayout, 1);
    rootLayout->addWidget(m_frame);

    setStyleSheet(Theme::macDialogStyleSheet());

    m_dismissTimer = new QTimer(this);
    m_dismissTimer->setSingleShot(true);
    connect(m_dismissTimer, &QTimer::timeout, this, &MacToast::fadeOutAndClose);
}

void MacToast::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setWindowBlur(reinterpret_cast<HWND>(winId()));
    Util::setWindowRoundCorner(reinterpret_cast<HWND>(winId()));
    setStyleSheet(Theme::macDialogStyleSheet());

    // 优雅入场淡入动效
    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(240);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    m_dismissTimer->start(m_durationMs);
}

void MacToast::fadeOutAndClose() {
    m_dismissTimer->stop();
    auto* anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(280);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::InCubic);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MacToast::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    m_dismissTimer->stop(); // 鼠标悬停时暂停自动消失
}

void MacToast::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_dismissTimer->start(2000); // 鼠标移开后 2 秒淡出
}

void MacToast::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_onClick) {
            m_onClick();
        }
        fadeOutAndClose();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MacToast::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void MacToast::showToast(
    const QString& title,
    const QString& message,
    Type type,
    int durationMs,
    std::function<void()> onClick
) {
    if (s_activeToast) {
        s_activeToast->fadeOutAndClose();
        s_activeToast = nullptr;
    }

    auto* toast = new MacToast(title, message, type, durationMs, std::move(onClick));
    s_activeToast = toast;

    QScreen* screen = (CFG.getDisplayMonitor() == PrimaryMonitor)
        ? QGuiApplication::primaryScreen()
        : QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
    constexpr int margin = 24;
    const int x = area.right() - toast->width() - margin;
    const int y = area.bottom() - toast->height() - margin;
    toast->move(qMax(area.left(), x), qMax(area.top(), y));

    toast->show();
}
