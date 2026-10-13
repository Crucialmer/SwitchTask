#ifndef WIN_SWITCHER_TASKBARWHEELHOOKER_H
#define WIN_SWITCHER_TASKBARWHEELHOOKER_H

#include <QObject>
#include <Windows.h>

class TaskbarWheelHooker : public QObject {
    Q_OBJECT

public:
    TaskbarWheelHooker();
    ~TaskbarWheelHooker() override;
    inline static TaskbarWheelHooker* instance = nullptr;

signals:
    /// 钩子线程发出（仅缓存事件，不含耗时逻辑），由本对象以 QueuedConnection 转入 GUI 线程
    void rawWheelEvent(bool isUp, HWND topLevelHwnd);
    void tabWheelEvent(const QString& exePath, bool isUp, int windows); // 参数为引用问题也不大，貌似会自动拷贝（Qt::QueuedConnection情况下）
    void leaveTaskbar(); // 鼠标离开taskbar

private slots:
    /// 在 GUI 线程执行原先钩子回调内的耗时逻辑（UIA查询/路径解析）
    void processWheelEvent(bool isUp, HWND topLevelHwnd);

private:
    HHOOK h_mouse = nullptr;
    bool isLastTaskbar = false; // 光标是否停留在任务栏（替代原函数内 static：跨实例共享、析构不复位）
    int hookFailCount = 0; // WH_MOUSE_LL 安装失败计数（限次重试，避免每 tick 重刷日志）
};


#endif //WIN_SWITCHER_TASKBARWHEELHOOKER_H