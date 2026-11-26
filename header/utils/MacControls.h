#ifndef WIN_SWITCHER_MACCONTROLS_H
#define WIN_SWITCHER_MACCONTROLS_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QStringList>
#include <QFontMetrics>
#include "Theme.h"
#include "Util.h"

/// macOS 26 风格类别图标徽标（28x28 连续曲率超椭圆方圆角，内置柔和顶部高光渐变与抗锯齿矢量符号）
class MacCategoryBadge : public QWidget {
public:
    enum IconType { Rocket, Monitor, Palette, Gear, Window };

    MacCategoryBadge(IconType type, const QColor& bgColor, QWidget* parent = nullptr)
        : QWidget(parent), m_type(type), m_bgColor(bgColor) {
        setFixedSize(28, 28);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // macOS 26 连续曲率圆角矩形底衬 + 顶部柔和立体光泽
        const QRectF r(0.5, 0.5, 27.0, 27.0);
        const QPainterPath path = Util::continuousRoundedRectPath(r, 7.5);
        QLinearGradient grad(r.topLeft(), r.bottomLeft());
        grad.setColorAt(0.0, m_bgColor.lighter(112));
        grad.setColorAt(1.0, m_bgColor.darker(106));
        p.fillPath(path, grad);

        // 顶部 Liquid Glass 微光内描边
        QLinearGradient rim(r.topLeft(), r.bottomLeft());
        rim.setColorAt(0.0, QColor(255, 255, 255, 115));
        rim.setColorAt(1.0, QColor(255, 255, 255, 25));
        p.setPen(QPen(QBrush(rim), 1.0));
        p.drawPath(path);

        // 矢量白色符号绘制
        p.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        switch (m_type) {
        case Rocket: {
            p.drawLine(QPointF(14, 6), QPointF(14, 18));
            p.drawLine(QPointF(10, 11), QPointF(14, 6));
            p.drawLine(QPointF(18, 11), QPointF(14, 6));
            p.drawLine(QPointF(9, 18), QPointF(19, 18));
            p.drawLine(QPointF(14, 18), QPointF(14, 22));
            break;
        }
        case Monitor: {
            p.drawRoundedRect(QRectF(6, 7, 16, 11), 2, 2);
            p.drawLine(QPointF(14, 18), QPointF(14, 21));
            p.drawLine(QPointF(10, 21), QPointF(18, 21));
            break;
        }
        case Palette: {
            p.drawEllipse(QRectF(6, 6, 16, 16));
            p.setBrush(Qt::white);
            p.drawEllipse(QRectF(9.5, 9.5, 2.2, 2.2));
            p.drawEllipse(QRectF(14, 8.5, 2.2, 2.2));
            p.drawEllipse(QRectF(17, 12, 2.2, 2.2));
            break;
        }
        case Window: {
            p.drawRoundedRect(QRectF(6, 7, 16, 13), 2, 2);
            p.drawLine(QPointF(6, 11), QPointF(22, 11));
            p.drawPoint(QPointF(9, 9));
            p.drawPoint(QPointF(12, 9));
            break;
        }
        case Gear: {
            p.drawEllipse(QRectF(9, 9, 10, 10));
            p.drawPoint(QPointF(14, 6));
            p.drawPoint(QPointF(14, 22));
            p.drawPoint(QPointF(6, 14));
            p.drawPoint(QPointF(22, 14));
            break;
        }
        }
    }

private:
    IconType m_type;
    QColor m_bgColor;
};

/// macOS 26 (Tahoe / Liquid Glass) 原生胶囊滑动开关（MacToggleSwitch）
class MacToggleSwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal thumbProgress READ thumbProgress WRITE setThumbProgress)

public:
    explicit MacToggleSwitch(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(44, 24);
        setCursor(Qt::PointingHandCursor);

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(180);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_thumbProgress = v.toReal();
            update();
        });
    }

    bool isChecked() const { return m_checked; }

    void setChecked(bool checked, bool animated = true) {
        if (m_checked == checked) return;
        m_checked = checked;
        if (animated && isVisible()) {
            m_anim->stop();
            m_anim->setStartValue(m_thumbProgress);
            m_anim->setEndValue(m_checked ? 1.0 : 0.0);
            m_anim->start();
        } else {
            m_anim->stop();
            m_thumbProgress = m_checked ? 1.0 : 0.0;
            update();
        }
        emit toggled(m_checked);
    }

    qreal thumbProgress() const { return m_thumbProgress; }
    void setThumbProgress(qreal p) { m_thumbProgress = p; update(); }

signals:
    void toggled(bool checked);

protected:
    void enterEvent(QEnterEvent*) override { m_hovered = true; update(); }
    void leaveEvent(QEvent*) override { m_hovered = false; m_pressed = false; update(); }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_pressed && e->button() == Qt::LeftButton) {
            m_pressed = false;
            if (rect().contains(e->pos())) {
                setChecked(!m_checked, true);
            } else {
                update();
            }
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool dark = Theme::isDark();
        const QRectF trackRect(0.5, 0.5, 43.0, 23.0);

        // 胶囊外轨颜色插值（悬停微提亮）
        const QColor offTrack = dark
            ? QColor(255, 255, 255, m_hovered ? 58 : 44)
            : QColor(0, 0, 0, m_hovered ? 48 : 36);
        const QColor onTrack = m_hovered ? QColor(0x30, 0xD1, 0x58) : QColor(0x34, 0xC7, 0x59);

        const int r = qRound(offTrack.red() + (onTrack.red() - offTrack.red()) * m_thumbProgress);
        const int g = qRound(offTrack.green() + (onTrack.green() - offTrack.green()) * m_thumbProgress);
        const int b = qRound(offTrack.blue() + (onTrack.blue() - offTrack.blue()) * m_thumbProgress);
        const int a = qRound(offTrack.alpha() + (onTrack.alpha() - offTrack.alpha()) * m_thumbProgress);

        QPainterPath trackPath;
        trackPath.addRoundedRect(trackRect, 11.5, 11.5);
        p.fillPath(trackPath, QColor(r, g, b, a));

        // 外轨 Liquid Glass 内折光微边
        QLinearGradient trackRim(0, 0, 0, height());
        trackRim.setColorAt(0.0, dark ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 22));
        trackRim.setColorAt(1.0, dark ? QColor(255, 255, 255, 16) : QColor(255, 255, 255, 70));
        p.setPen(QPen(QBrush(trackRim), 1.0));
        p.drawPath(trackPath);

        // macOS 26 滑动玻璃旋钮：按压微拉伸 + 柔和投射阴影 + 顶部光泽高光
        const qreal thumbW = m_pressed ? 23.0 : 20.0;
        const qreal thumbH = 19.0;
        const qreal travel = 44.0 - 5.0 - thumbW;
        const qreal thumbX = 2.5 + travel * m_thumbProgress;
        const QRectF thumbRect(thumbX, 2.5, thumbW, thumbH);

        QPainterPath shadowPath;
        shadowPath.addRoundedRect(QRectF(thumbX, 3.4, thumbW, thumbH), thumbH * 0.5, thumbH * 0.5);
        p.fillPath(shadowPath, QColor(0, 0, 0, dark ? 68 : 42));

        QPainterPath thumbPath;
        thumbPath.addRoundedRect(thumbRect, thumbH * 0.5, thumbH * 0.5);
        QLinearGradient thumbGrad(thumbRect.topLeft(), thumbRect.bottomLeft());
        thumbGrad.setColorAt(0.0, QColor(255, 255, 255));
        thumbGrad.setColorAt(1.0, QColor(242, 244, 248));
        p.fillPath(thumbPath, thumbGrad);

        p.setPen(QPen(QColor(255, 255, 255, 220), 0.75));
        p.drawPath(thumbPath);
    }

private:
    bool m_checked = false;
    bool m_hovered = false;
    bool m_pressed = false;
    qreal m_thumbProgress = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

/// macOS 26 (Tahoe / Liquid Glass) 全胶囊分段选择器（MacSegmentedControl）
class MacSegmentedControl : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal activeProgress READ activeProgress WRITE setActiveProgress)

public:
    explicit MacSegmentedControl(const QStringList& items, QWidget* parent = nullptr)
        : QWidget(parent), m_items(items) {
        setFixedHeight(32);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true); // 启用悬停追踪，驱动悬停分段玻璃微光反馈

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(190);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_animPos = v.toReal();
            update();
        });
    }

    int currentIndex() const { return m_currentIndex; }

    void setCurrentIndex(int index, bool animated = true) {
        if (index < 0 || index >= m_items.size() || m_currentIndex == index) return;
        m_currentIndex = index;

        if (animated && isVisible()) {
            m_anim->stop();
            // 从当前实时动画位置无缝衔接，避免连点时滑块回跳
            m_anim->setStartValue(m_animPos);
            m_anim->setEndValue(qreal(m_currentIndex));
            m_anim->start();
        } else {
            m_anim->stop();
            m_animPos = qreal(m_currentIndex);
            update();
        }
        emit currentIndexChanged(m_currentIndex);
    }

    qreal activeProgress() const { return m_animPos; }
    void setActiveProgress(qreal p) { m_animPos = p; update(); }

signals:
    void currentIndexChanged(int index);

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && !m_items.isEmpty()) {
            const qreal segWidth = qreal(width()) / m_items.size();
            const int clickedIndex = qBound(0, int(e->position().x() / segWidth), m_items.size() - 1);
            setCurrentIndex(clickedIndex, true);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (!m_items.isEmpty()) {
            const qreal segWidth = qreal(width()) / m_items.size();
            const int hover = qBound(0, int(e->position().x() / segWidth), m_items.size() - 1);
            if (m_hoverIndex != hover) {
                m_hoverIndex = hover;
                update();
            }
        }
    }

    void leaveEvent(QEvent*) override {
        if (m_hoverIndex != -1) {
            m_hoverIndex = -1;
            update();
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (m_items.isEmpty()) return;

        const bool dark = Theme::isDark();
        const QRectF totalRect(0.5, 0.5, width() - 1.0, height() - 1.0);
        const qreal trackRadius = totalRect.height() * 0.5;

        // 1. macOS 26 全胶囊磨砂底轨
        QPainterPath bgPath;
        bgPath.addRoundedRect(totalRect, trackRadius, trackRadius);
        p.fillPath(bgPath, dark ? QColor(0, 0, 0, 68) : QColor(0, 0, 0, 15));
        p.setPen(QPen(dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 16), 1.0));
        p.drawPath(bgPath);

        const int count = m_items.size();
        const qreal pad = 2.5;
        const qreal segWidth = qreal(width() - pad * 2.0) / count;
        const qreal pillH = height() - pad * 2.0;
        const qreal pillRadius = pillH * 0.5;

        // 2. 非选中分段的悬停轻玻璃提示
        if (m_hoverIndex >= 0 && m_hoverIndex < count && m_hoverIndex != m_currentIndex) {
            const QRectF hoverRect(pad + m_hoverIndex * segWidth, pad, segWidth, pillH);
            QPainterPath hoverPath;
            hoverPath.addRoundedRect(hoverRect, pillRadius, pillRadius);
            p.fillPath(hoverPath, dark ? QColor(255, 255, 255, 16) : QColor(255, 255, 255, 110));
        }

        // 3. 滑动激活 Liquid Glass 胶囊透镜（Active Glass Pill）
        const qreal pillX = pad + m_animPos * segWidth;
        const QRectF pillRect(pillX, pad, segWidth, pillH);

        QPainterPath shadowPath;
        shadowPath.addRoundedRect(QRectF(pillX, pad + 0.9, segWidth, pillH), pillRadius, pillRadius);
        p.fillPath(shadowPath, QColor(0, 0, 0, dark ? 56 : 26));

        QPainterPath pillPath;
        pillPath.addRoundedRect(pillRect, pillRadius, pillRadius);
        QLinearGradient pillGrad(pillRect.topLeft(), pillRect.bottomLeft());
        if (dark) {
            pillGrad.setColorAt(0.0, QColor(255, 255, 255, 62));
            pillGrad.setColorAt(1.0, QColor(255, 255, 255, 40));
        } else {
            pillGrad.setColorAt(0.0, QColor(255, 255, 255, 250));
            pillGrad.setColorAt(1.0, QColor(246, 248, 252, 236));
        }
        p.fillPath(pillPath, pillGrad);

        QLinearGradient pillRim(pillRect.topLeft(), pillRect.bottomLeft());
        if (dark) {
            pillRim.setColorAt(0.0, QColor(255, 255, 255, 92));
            pillRim.setColorAt(1.0, QColor(255, 255, 255, 32));
        } else {
            pillRim.setColorAt(0.0, QColor(255, 255, 255, 255));
            pillRim.setColorAt(1.0, QColor(0, 0, 0, 22));
        }
        p.setPen(QPen(QBrush(pillRim), 1.0));
        p.drawPath(pillPath);

        // 4. 各段标签文字
        QFont font = p.font();
        font.setFamily(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(11);

        for (int i = 0; i < count; ++i) {
            const QRectF itemRect(pad + i * segWidth, 0, segWidth, height());
            const bool isSelected = (i == m_currentIndex);
            const bool isHovered = (i == m_hoverIndex);

            font.setBold(isSelected);
            p.setFont(font);

            QColor textColor;
            if (isSelected) {
                textColor = dark ? QColor(0xF5, 0xF5, 0xF7) : QColor(0x1C, 0x1C, 0x1E);
            } else if (isHovered) {
                textColor = dark ? QColor(0xD8, 0xD8, 0xDF) : QColor(0x3A, 0x3A, 0x3C);
            } else {
                textColor = dark ? QColor(0xA1, 0xA1, 0xA6) : QColor(0x6E, 0x6E, 0x73);
            }

            p.setPen(textColor);
            p.drawText(itemRect, Qt::AlignCenter, m_items[i]);
        }
    }

private:
    QStringList m_items;
    int m_currentIndex = 0;
    int m_hoverIndex = -1;
    qreal m_animPos = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

#endif // WIN_SWITCHER_MACCONTROLS_H
