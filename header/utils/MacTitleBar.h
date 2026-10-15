#ifndef WIN_SWITCHER_MACTITLEBAR_H
#define WIN_SWITCHER_MACTITLEBAR_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QPoint>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include <functional>
#include "Theme.h"

/// macOS 交通灯控制按钮（红灯关闭、黄灯最小化、绿灯缩放）
class MacTrafficButton : public QWidget {
public:
    enum Type { Close, Minimize, Zoom };
    std::function<void()> onClick;

    explicit MacTrafficButton(Type type, QWidget* parent = nullptr)
        : QWidget(parent), m_type(type) {
        setFixedSize(13, 13);
        setCursor(Qt::ArrowCursor);
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
        if (m_pressed && e->button() == Qt::LeftButton && rect().contains(e->pos())) {
            m_pressed = false;
            update();
            if (onClick) onClick();
        }
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor baseColor, borderColor;
        switch (m_type) {
        case Close:
            baseColor = m_pressed ? QColor(0xBF, 0x36, 0x30) : (m_hover ? QColor(0xE0, 0x44, 0x3E) : QColor(0xFF, 0x5F, 0x56));
            borderColor = QColor(0xE0, 0x44, 0x3E);
            break;
        case Minimize:
            baseColor = m_pressed ? QColor(0xC2, 0x7E, 0x1A) : (m_hover ? QColor(0xDE, 0xA1, 0x23) : QColor(0xFF, 0xBD, 0x2E));
            borderColor = QColor(0xDE, 0xA1, 0x23);
            break;
        case Zoom:
            baseColor = m_pressed ? QColor(0x1B, 0x82, 0x25) : (m_hover ? QColor(0x1A, 0xAB, 0x29) : QColor(0x27, 0xC9, 0x3F));
            borderColor = QColor(0x1A, 0xAB, 0x29);
            break;
        }

        const QRectF r(0.5, 0.5, 12, 12);
        p.setPen(borderColor);
        p.setBrush(baseColor);
        p.drawEllipse(r);

        // 悬停显示 macOS 经典符号
        if (m_hover) {
            if (m_type == Close) {
                p.setPen(QPen(QColor(77, 0, 0, 180), 1.2, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(4.0, 4.0), QPointF(9.0, 9.0));
                p.drawLine(QPointF(9.0, 4.0), QPointF(4.0, 9.0));
            } else if (m_type == Minimize) {
                p.setPen(QPen(QColor(153, 87, 0, 180), 1.2, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(3.5, 6.5), QPointF(9.5, 6.5));
            } else if (m_type == Zoom) {
                p.setPen(QPen(QColor(0, 102, 0, 180), 1.2, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(6.5, 3.5), QPointF(6.5, 9.5));
                p.drawLine(QPointF(3.5, 6.5), QPointF(9.5, 6.5));
            }
        }
    }

private:
    Type m_type;
    bool m_hover = false;
    bool m_pressed = false;
};

/// macOS 26 原生自绘标题栏：红黄绿三色交通灯 + 居中标题 + 平滑拖拽移动窗口
class MacTitleBar : public QWidget {
public:
    explicit MacTitleBar(const QString& title, QWidget* parentWindow = nullptr)
        : QWidget(parentWindow), m_window(parentWindow) {
        setFixedHeight(38);
        setObjectName("macTitleBar");

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(14, 0, 14, 0);
        layout->setSpacing(8);

        m_closeBtn = new MacTrafficButton(MacTrafficButton::Close, this);
        m_minBtn = new MacTrafficButton(MacTrafficButton::Minimize, this);
        m_zoomBtn = new MacTrafficButton(MacTrafficButton::Zoom, this);

        m_closeBtn->onClick = [this] {
            if (m_window) m_window->close();
        };
        m_minBtn->onClick = [this] {
            if (m_window) m_window->showMinimized();
        };
        m_zoomBtn->onClick = [this] {
            if (m_window) {
                if (m_window->isMaximized()) m_window->showNormal();
                else m_window->showMaximized();
            }
        };

        layout->addWidget(m_closeBtn);
        layout->addWidget(m_minBtn);
        layout->addWidget(m_zoomBtn);

        m_titleLabel = new QLabel(title, this);
        m_titleLabel->setObjectName("macTitleBarTitle");
        m_titleLabel->setAlignment(Qt::AlignCenter);

        // 左右配平宽度，使标题严格水平居中
        layout->addWidget(m_titleLabel, 1);

        auto* rightSpacer = new QWidget(this);
        rightSpacer->setFixedSize(13 * 3 + 8 * 2, 13);
        layout->addWidget(rightSpacer);
    }

    void setTitle(const QString& title) {
        if (m_titleLabel) m_titleLabel->setText(title);
    }

    /// 将窗口摆放到屏幕工作区右下角（贴近系统托盘区）
    static void positionBottomRight(QWidget* window, int margin = 24) {
        if (!window) return;
        QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
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
        p.setPen(Theme::isDark() ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 18));
        p.drawLine(0, height() - 1, width(), height() - 1);
    }

private:
    QWidget* m_window = nullptr;
    MacTrafficButton* m_closeBtn = nullptr;
    MacTrafficButton* m_minBtn = nullptr;
    MacTrafficButton* m_zoomBtn = nullptr;
    QLabel* m_titleLabel = nullptr;
    QPoint m_dragPos;
    bool m_dragging = false;
};

#endif // WIN_SWITCHER_MACTITLEBAR_H
