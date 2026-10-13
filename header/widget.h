#ifndef WIN_SWITCHER_WIDGET_H
#define WIN_SWITCHER_WIDGET_H

#include <QWidget>
#include <Windows.h>
#include <QListWidget>
#include <QDebug>
#include <QDateTime>

struct WindowInfo {
    QString title;
    QString className;
    HWND hwnd = nullptr;
    QString exePath; // 所属进程路径（图标回退/标签兜底用）
};

// C++17 inline: 防止重定义
inline QDebug operator<<(QDebug dbg, const WindowInfo& info) {
    dbg.nospace() << "WindowInfo(" << info.title << ", " << info.className << ", " << info.hwnd << ", " << info.exePath << ")";
    return dbg.space();
}

Q_DECLARE_METATYPE(WindowInfo) // for QVariant

QT_BEGIN_NAMESPACE

namespace Ui {
    class Widget;
}

QT_END_NAMESPACE

class IconOnlyDelegate;
class QVariantAnimation;

class Widget : public QWidget {
    Q_OBJECT

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

public:
    enum ForegroundChangeSource {
        WinEvent,
        Inner,
    };

    Q_ENUM(ForegroundChangeSource) // for QMetaEnum, to QString

public:
    explicit Widget(QWidget* parent = nullptr);
    QList<WindowInfo> prepareWindowList(); // 每个窗口独立一项
    bool prepareListWidget();
    Q_INVOKABLE bool requestShow();
    Q_INVOKABLE void requestShowPinned(); // Ctrl+Alt+Tab：松开按键后保持显示，Enter/点击确认，Esc 取消
    void notifyForegroundChanged(HWND hwnd, ForegroundChangeSource source);

    HWND hWnd() { return (HWND) winId(); }

    bool isForeground() { return GetForegroundWindow() == hWnd(); }

    ~Widget() override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows);
    void clearGroupWindowOrder();

private:
    bool forceShow();
    /// 执行"切换到当前选中项"并隐藏（Alt释放与pinned确认共用）
    void switchToCurrentItem();
    /// 单击图标：选中并立即切换（鼠标确认路径；事件过滤器与 pressed 信号兜底共用）
    void handleIconClick(QListWidgetItem* item);
    /// 中键单击图标：关闭该窗口（WM_CLOSE 优雅关闭）并移除条目（事件过滤器与 pressed 信号兜底共用）
    void closeWindowItem(QListWidgetItem* item);
    /// Ctrl+W：关闭当前选中项对应的窗口（与中键单击等效）
    void closeCurrentItem();
    /// macOS 风格：播放选中项放大动效（选中变化 / 弹窗显示时触发）
    void startSelectionGrow();
    /// 循环选择（forward=true 下一个 / false 上一个）
    void cycleSelection(bool forward);
    void showLabelForItem(QListWidgetItem* item);
    void setupLabelFont();
    void sortGroupWindows(QList<HWND>& windows, const QString& exePath);
    QList<HWND> buildGroupWindowOrder(const QString& exePath);
    static HWND rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);
    static HWND rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward = true);

private:
    Ui::Widget* ui;
    QListWidget* lw = nullptr;
    IconOnlyDelegate* iconDelegate = nullptr; // 列表 delegate（驱动选中放大动效）
    QVariantAnimation* selectionGrowAnim = nullptr; // macOS 风格：选中项放大动画 0→1
    const QMargins ListWidgetMargin{24, 24, 24, 24}; // 左/上/右参与布局；bottom 为 marginsAdded 占位（底边距由“文字等距”规则动态计算，见 prepareListWidget）
    /// exePath -> (HWND, time)
    QHash<QString, QHash<HWND, QDateTime>> winActiveOrder;
    QList<HWND> groupWindowOrder; // for Alt+` 同组窗口切换

    // --- 状态：替代原函数内 static（跨调用存活、存在悬垂/污染风险）---
    // rotateTaskbarWindowInGroup 状态
    QString taskbarLastPath;
    HWND taskbarLastHwnd = nullptr;
    bool isLastTaskbarForward = true;
    // Ctrl+Alt+Tab pinned 模式：松开按键后保持显示，Enter/点击确认，Esc 取消
    bool pinned = false;
};


#endif //WIN_SWITCHER_WIDGET_H
