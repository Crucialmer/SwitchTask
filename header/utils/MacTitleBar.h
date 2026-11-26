#ifndef WIN_SWITCHER_MACTITLEBAR_H
#define WIN_SWITCHER_MACTITLEBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QPoint>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include <functional>
#include "Theme.h"
#include "ConfigManager.h"

/// macOS 交通灯控制按钮（红灯关闭、黄灯最小化、绿灯缩放；支持原厂三灯联动组悬停显符）
class MacTrafficButton : public QWidget {
public:
    enum Type { Close, Minimize, Zoom };
    std::function<void()> onClick;

    explicit MacTrafficButton(Type type, QWidget* parent = nullptr)
        : QWidget(parent), m_type(type) {
        setFixedSize(13, 13);
        setCursor(Qt::ArrowCursor);
    }

    void setGroupHovered(bool hovered) {
        if (m_groupHover != hovered) {
            m_groupHover = hovered;
            update();
        }
    }

protected:
    void enterEvent(QEnterEvent*) override {
        m_hover = true;
        update();
    }
    void leaveEvent(QEvent*) override {
        m_hover = false;
        m_pressed = false;
        update();
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (m_pressed && e->button() == Qt::LeftButton) {
            m_pressed = false;
            update();
            if (rect().contains(e->pos()) && onClick) onClick();
        }
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor baseColor, borderColor;
        switch (m_type) {
        case Close:
            baseColor = m_pressed ? QColor(0xBF, 0x36, 0x30) : (m_hover ? QColor(0xE8, 0x4C, 0x45) : QColor(0xFF, 0x5F, 0x57));
            borderColor = QColor(0xE0, 0x44, 0x3E);
            break;
        case Minimize:
            baseColor = m_pressed ? QColor(0xC2, 0x7E, 0x1A) : (m_hover ? QColor(0xE5, 0xA8, 0x26) : QColor(0xFE, 0xBC, 0x2E));
            borderColor = QColor(0xDE, 0xA1, 0x23);
            break;
        case Zoom:
            baseColor = m_pressed ? QColor(0x1B, 0x82, 0x25) : (m_hover ? QColor(0x1F, 0xB2, 0x36) : QColor(0x28, 0xC8, 0x40));
            borderColor = QColor(0x1A, 0xAB, 0x29);
            break;
        }

        const QRectF r(0.5, 0.5, 12.0, 12.0);
        QLinearGradient gloss(r.topLeft(), r.bottomLeft());
        gloss.setColorAt(0.0, baseColor.lighter(108));
        gloss.setColorAt(1.0, baseColor);
        p.setPen(QPen(borderColor, 0.8));
        p.setBrush(gloss);
        p.drawEllipse(r);

        // macOS 原生组悬停：悬停任意交通灯时三灯同时显露极细抗锯齿符号
        if (m_hover || m_groupHover) {
            if (m_type == Close) {
                p.setPen(QPen(QColor(77, 0, 0, 195), 1.25, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(4.1, 4.1), QPointF(8.9, 8.9));
                p.drawLine(QPointF(8.9, 4.1), QPointF(4.1, 8.9));
            } else if (m_type == Minimize) {
                p.setPen(QPen(QColor(153, 87, 0, 195), 1.25, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(3.6, 6.5), QPointF(9.4, 6.5));
            } else if (m_type == Zoom) {
                p.setPen(QPen(QColor(0, 102, 0, 195), 1.25, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(6.5, 3.6), QPointF(6.5, 9.4));
                p.drawLine(QPointF(3.6, 6.5), QPointF(9.4, 6.5));
            }
        }
    }

private:
    Type m_type;
    bool m_hover = false;
    bool m_groupHover = false;
    bool m_pressed = false;
};

/// 交通灯联动容器：统一捕获三灯区域的进入/离开事件，驱动同步显符
class MacTrafficLightGroup : public QWidget {
public:
    explicit MacTrafficLightGroup(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);

        closeBtn = new MacTrafficButton(MacTrafficButton::Close, this);
        minBtn = new MacTrafficButton(MacTrafficButton::Minimize, this);
        zoomBtn = new MacTrafficButton(MacTrafficButton::Zoom, this);

        layout->addWidget(closeBtn);
        layout->addWidget(minBtn);
        layout->addWidget(zoomBtn);
        setFixedSize(13 * 3 + 8 * 2, 13);
    }

    MacTrafficButton* closeBtn = nullptr;
    MacTrafficButton* minBtn = nullptr;
    MacTrafficButton* zoomBtn = nullptr;

protected:
    void enterEvent(QEnterEvent* e) override {
        QWidget::enterEvent(e);
        setAllHovered(true);
    }
    void leaveEvent(QEvent* e) override {
        QWidget::leaveEvent(e);
        setAllHovered(false);
    }

private:
    void setAllHovered(bool hovered) {
        closeBtn->setGroupHovered(hovered);
        minBtn->setGroupHovered(hovered);
        zoomBtn->setGroupHovered(hovered);
    }
};

/// macOS 26 原生自绘标题栏：三灯联动交通灯 + 居中标题 + 平滑拖拽移动窗口
class MacTitleBar : public QWidget {
public:
    explicit MacTitleBar(const QString& title, QWidget* parentWindow = nullptr)
        : QWidget(parentWindow), m_window(parentWindow) {
        setFixedHeight(40);
        setObjectName("macTitleBar");

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);
        layout->setSpacing(8);

        m_lights = new MacTrafficLightGroup(this);
        m_lights->closeBtn->onClick = [this] {
            if (m_window) m_window->close();
        };
        m_lights->minBtn->onClick = [this] {
            if (m_window) m_window->showMinimized();
        };
        m_lights->zoomBtn->onClick = [this] {
            if (m_window) {
                if (m_window->isMaximized()) m_window->showNormal();
                else m_window->showMaximized();
            }
        };

        layout->addWidget(m_lights, 0, Qt::AlignVCenter);

        m_titleLabel = new QLabel(title, this);
        m_titleLabel->setObjectName("macTitleBarTitle");
        m_titleLabel->setAlignment(Qt::AlignCenter);

        // 左右配平宽度，使标题严格水平居中
        layout->addWidget(m_titleLabel, 1);

        auto* rightSpacer = new QWidget(this);
        rightSpacer->setFixedSize(m_lights->size());
        layout->addWidget(rightSpacer, 0, Qt::AlignVCenter);
    }

    void setTitle(const QString& title) {
        if (m_titleLabel) m_titleLabel->setText(title);
    }

    /// 将窗口摆放到屏幕工作区右下角（贴近系统托盘区，支持多屏混合 DPI 感知）
    static void positionBottomRight(QWidget* window, int margin = 24) {
        if (!window) return;
        QScreen* screen = (CFG.getDisplayMonitor() == PrimaryMonitor)
            ? QGuiApplication::primaryScreen()
            : QGuiApplication::screenAt(QCursor::pos());
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
        const int x = area.right() - window->width() - margin;
        const int y = area.bottom() - window->height() - margin;
        window->move(qMax(area.left(), x), qMax(area.top(), y));
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragPos = e->globalPosition().toPoint() - (m_window ? m_window->frameGeometry().topLeft() : QPoint());
            m_dragging = true;
        }
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if (m_dragging && m_window && (e->buttons() & Qt::LeftButton)) {
            m_window->move(e->globalPosition().toPoint() - m_dragPos);
        }
    }
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            m_dragging = false;
        }
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(Theme::isDark() ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 16));
        p.drawLine(14, height() - 1, width() - 14, height() - 1);
    }

private:
    QWidget* m_window = nullptr;
    MacTrafficLightGroup* m_lights = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};

#endif // WIN_SWITCHER_MACTITLEBAR_H
