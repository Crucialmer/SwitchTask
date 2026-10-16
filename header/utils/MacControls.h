#ifndef WIN_SWITCHER_MACCONTROLS_H
#define WIN_SWITCHER_MACCONTROLS_H

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QStringList>
#include <QFontMetrics>
#include "Theme.h"

/// macOS 风格类别图标徽标（28x28 连续曲率方圆角，内置柔和背景与抗锯齿矢量符号）
class MacCategoryBadge : public QWidget {
public:
    enum IconType { Rocket, Monitor, Palette, Gear };

    MacCategoryBadge(IconType type, const QColor& bgColor, QWidget* parent = nullptr)
        : QWidget(parent), m_type(type), m_bgColor(bgColor) {
        setFixedSize(28, 28);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // 连续曲率圆角矩形底衬
        QPainterPath path;
        path.addRoundedRect(QRectF(0, 0, 28, 28), 7, 7);
        p.fillPath(path, m_bgColor);

        // 微光内发光描边
        p.setPen(QPen(QColor(255, 255, 255, 50), 1.0));
        p.drawPath(path);

        // 矢量白色符号绘制
        p.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        switch (m_type) {
        case Rocket: { // 启动/火箭小图标
            p.drawLine(QPointF(14, 6), QPointF(14, 18));
            p.drawLine(QPointF(10, 11), QPointF(14, 6));
            p.drawLine(QPointF(18, 11), QPointF(14, 6));
            p.drawLine(QPointF(9, 18), QPointF(19, 18));
            p.drawLine(QPointF(14, 18), QPointF(14, 22));
            break;
        }
        case Monitor: { // 显示器图标
            p.drawRoundedRect(QRectF(6, 7, 16, 11), 2, 2);
            p.drawLine(QPointF(14, 18), QPointF(14, 21));
            p.drawLine(QPointF(10, 21), QPointF(18, 21));
            break;
        }
        case Palette: { // 调色板/画板图标
            p.drawEllipse(QRectF(6, 6, 16, 16));
            p.setBrush(Qt::white);
            p.drawEllipse(QRectF(9.5, 9.5, 2.2, 2.2));
            p.drawEllipse(QRectF(14, 8.5, 2.2, 2.2));
            p.drawEllipse(QRectF(17, 12, 2.2, 2.2));
            break;
        }
        case Gear: { // 齿轮/设置图标
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

/// macOS 风格原生胶囊滑动开关（MacToggleSwitch，对标 macOS Sequoia System Settings）
class MacToggleSwitch : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal thumbProgress READ thumbProgress WRITE setThumbProgress)

public:
    explicit MacToggleSwitch(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(42, 24);
        setCursor(Qt::PointingHandCursor);

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(160);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
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
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            setChecked(!m_checked, true);
        }
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const bool dark = Theme::isDark();
        const QRectF trackRect(0.5, 0.5, 41, 23);

        // 胶囊外轨颜色插值
        QColor offTrack = dark ? QColor(255, 255, 255, 45) : QColor(0, 0, 0, 38);
        QColor onTrack(0x34, 0xC7, 0x59); // Apple Traffic Green

        int r = int(offTrack.red() + (onTrack.red() - offTrack.red()) * m_thumbProgress);
        int g = int(offTrack.green() + (onTrack.green() - offTrack.green()) * m_thumbProgress);
        int b = int(offTrack.blue() + (onTrack.blue() - offTrack.blue()) * m_thumbProgress);
        int a = int(offTrack.alpha() + (onTrack.alpha() - offTrack.alpha()) * m_thumbProgress);
        QColor currentTrack(r, g, b, a);

        // 绘制胶囊外轨
        QPainterPath trackPath;
        trackPath.addRoundedRect(trackRect, 11.5, 11.5);
        p.fillPath(trackPath, currentTrack);

        // 绘制微光边框
        p.setPen(QPen(dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 20), 1.0));
        p.drawPath(trackPath);

        // 计算圆形滑动旋钮（Thumb）位置：未激活 x=2.5，激活 x=20.5
        const qreal thumbX = 2.5 + 18.0 * m_thumbProgress;
        const QRectF thumbRect(thumbX, 2.5, 19, 19);

        // 旋钮阴影
        QPainterPath shadowPath;
        shadowPath.addEllipse(QRectF(thumbX, 3.2, 19, 19));
        p.fillPath(shadowPath, QColor(0, 0, 0, dark ? 60 : 40));

        // 旋钮本体（纯白饱满实心球）
        QPainterPath thumbPath;
        thumbPath.addEllipse(thumbRect);
        p.fillPath(thumbPath, Qt::white);
    }

private:
    bool m_checked = false;
    qreal m_thumbProgress = 0.0;
    QVariantAnimation* m_anim = nullptr;
};

/// macOS 风格原生分段胶囊选择器（MacSegmentedControl，对标 macOS 26 多段选择器）
class MacSegmentedControl : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal activeProgress READ activeProgress WRITE setActiveProgress)

public:
    explicit MacSegmentedControl(const QStringList& items, QWidget* parent = nullptr)
        : QWidget(parent), m_items(items) {
        setFixedHeight(32);
        setCursor(Qt::PointingHandCursor);

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(160);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_animPos = v.toReal();
            update();
        });
    }

    int currentIndex() const { return m_currentIndex; }

    void setCurrentIndex(int index, bool animated = true) {
        if (index < 0 || index >= m_items.size() || m_currentIndex == index) return;
        const int prev = m_currentIndex;
        m_currentIndex = index;

        if (animated && isVisible()) {
            m_anim->stop();
            m_anim->setStartValue(qreal(prev));
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
            int hover = qBound(0, int(e->position().x() / segWidth), m_items.size() - 1);
            if (m_hoverIndex != hover) {
                m_hoverIndex = hover;
                update();
            }
        }
    }

    void leaveEvent(QEvent*) override {
        m_hoverIndex = -1;
        update();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (m_items.isEmpty()) return;

        const bool dark = Theme::isDark();
        const QRectF totalRect(0.5, 0.5, width() - 1, height() - 1);

        // 底轨矩形（macOS 磨砂衬底）
        QPainterPath bgPath;
        bgPath.addRoundedRect(totalRect, 7, 7);
        p.fillPath(bgPath, dark ? QColor(0, 0, 0, 60) : QColor(0, 0, 0, 16));
        p.setPen(QPen(dark ? QColor(255, 255, 255, 18) : QColor(0, 0, 0, 14), 1.0));
        p.drawPath(bgPath);

        const int count = m_items.size();
        const qreal segWidth = qreal(width() - 4) / count;

        // 绘制滑动激活指示胶囊（Active Pill）
        const qreal pillX = 2.0 + m_animPos * segWidth;
        const QRectF pillRect(pillX, 2.0, segWidth, height() - 4);

        // 阴影
        QPainterPath shadowPath;
        shadowPath.addRoundedRect(QRectF(pillX, 2.8, segWidth, height() - 4), 5.5, 5.5);
        p.fillPath(shadowPath, QColor(0, 0, 0, dark ? 50 : 25));

        // 胶囊本体
        QPainterPath pillPath;
        pillPath.addRoundedRect(pillRect, 5.5, 5.5);
        p.fillPath(pillPath, dark ? QColor(255, 255, 255, 48) : QColor(255, 255, 255, 240));
        p.setPen(QPen(dark ? QColor(255, 255, 255, 55) : QColor(0, 0, 0, 20), 1.0));
        p.drawPath(pillPath);

        // 绘制各段标签文字
        QFont font = p.font();
        font.setFamily(QStringLiteral("Microsoft YaHei UI"));
        font.setPixelSize(11);

        for (int i = 0; i < count; ++i) {
            const QRectF itemRect(2.0 + i * segWidth, 0, segWidth, height());
            const bool isSelected = (i == m_currentIndex);

            font.setBold(isSelected);
            p.setFont(font);

            QColor textColor;
            if (isSelected) {
                textColor = dark ? QColor(0xF5, 0xF5, 0xF7) : QColor(0x1C, 0x1C, 0x1E);
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
