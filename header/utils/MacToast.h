#ifndef WIN_SWITCHER_MACTOAST_H
#define WIN_SWITCHER_MACTOAST_H

#include <QWidget>
#include <QString>
#include <functional>
#include <QPointer>

class QFrame;
class QLabel;
class QTimer;

/// macOS 26 风格自绘原生悬浮气泡通知（代替 Windows 11 传统系统消息弹窗）
/// 具备 DWM 亚克力毛玻璃、12px 平滑大圆角、微光描边、平滑淡入淡出动画与无感交互（不抢夺前台焦点）
class MacToast : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal windowOpacity READ windowOpacity WRITE setWindowOpacity)

public:
    enum Type {
        Success, // 成功：绿色对勾微标
        Info,    // 信息：蓝色提示微标
        Warning  // 警告：橙色感叹号微标
    };

    explicit MacToast(
        const QString& title,
        const QString& message,
        Type type = Success,
        int durationMs = 4500,
        std::function<void()> onClick = nullptr,
        QWidget* parent = nullptr
    );
    ~MacToast() override = default;

    /// 弹出 macOS 风格悬浮气泡通知
    static void showToast(
        const QString& title,
        const QString& message,
        Type type = Success,
        int durationMs = 4500,
        std::function<void()> onClick = nullptr
    );

protected:
    void showEvent(QShowEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void fadeOutAndClose();

    QFrame* m_frame = nullptr;
    QTimer* m_dismissTimer = nullptr;
    int m_durationMs = 4500;
    std::function<void()> m_onClick;

    static inline QPointer<MacToast> s_activeToast;
};

#endif // WIN_SWITCHER_MACTOAST_H
