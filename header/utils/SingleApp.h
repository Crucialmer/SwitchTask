#ifndef WIN_SWITCHER_SINGLEAPP_H
#define WIN_SWITCHER_SINGLEAPP_H

#include <Windows.h>
#include <QString>
#include <QDebug>

class SingleApp {
private:
    HANDLE m_hMutex = nullptr;
    bool m_isRunning = false;

public:
    explicit SingleApp(const QString& key) {
        // 使用 Windows 内核命名互斥体实现原子单实例保护：
        // 彻底根除 QSharedMemory 的 TOCTOU 竞态，并在程序异常崩溃时由系统内核自动回收句柄，绝无残留
        const QString mutexName = QStringLiteral("Local\\SwitchTask_App_%1").arg(key);
        m_hMutex = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(mutexName.utf16()));
        if (m_hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
            m_isRunning = true;
        }
    }

    ~SingleApp() {
        if (m_hMutex) {
            CloseHandle(m_hMutex);
            m_hMutex = nullptr;
        }
    }

    /// check if another instance is running
    bool isRunning() const {
        return m_isRunning;
    }
};

#endif //WIN_SWITCHER_SINGLEAPP_H
