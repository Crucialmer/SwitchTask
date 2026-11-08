#ifndef WIN_SWITCHER_TASKBARWHEELHOOKER_H
#define WIN_SWITCHER_TASKBARWHEELHOOKER_H

#include <QObject>
#include <Windows.h>
#include <thread>

class TaskbarWheelHooker : public QObject {
    Q_OBJECT

public:
    TaskbarWheelHooker();
    ~TaskbarWheelHooker() override;
    inline static TaskbarWheelHooker* instance = nullptr;

signals:
    /// 钩子专属线程发出（仅缓存事件，不含耗时逻辑），由本对象以 QueuedConnection 转入 GUI 线程
    void rawWheelEvent(bool isUp, HWND topLevelHwnd);
    void tabWheelEvent(const QString& exePath, bool isUp, int windows);
    void leaveTaskbar();

private slots:
    /// 在 GUI 线程执行原先钩子回调内的耗时逻辑（UIA查询/路径解析）
    void processWheelEvent(bool isUp, HWND topLevelHwnd);

private:
    std::thread m_thread;
    DWORD m_threadId = 0;
};

#endif //WIN_SWITCHER_TASKBARWHEELHOOKER_H
