#ifndef WIN_SWITCHER_STARTUP_H
#define WIN_SWITCHER_STARTUP_H

#include <QString>
#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QMessageBox>
#include <shlobj_core.h>

// ScheduledTask.h 可选，作为一个插件，用以支持以[管理员权限]自启动
#if __has_include("ScheduledTask.h")
# define HAS_SCHTASK
# include "ScheduledTask.h"
#else
# warning "ScheduledTask.h not found, use registry instead."
#endif

class Startup {
public:
    Startup() = delete;
    friend class SystemTray;

    /// 状态查询（带缓存）：schtask 查询需 fork 子进程（10ms~秒级），
    /// 托盘菜单 aboutToShow 等高频调用点不应阻塞 GUI 线程。
    /// 缓存策略：结果缓存 60 秒（schtasks 查询需 fork 子进程，托盘菜单弹出不应频繁等待）。
    /// 状态可能被本程序、任务计划程序或系统优化工具改动：本程序写操作后按已知终态直接写回
    /// （setCacheKnown）或立即刷新（invalidateCache）；外部改动最长 60 秒后才反映。
    static bool isOn(bool refresh = false) {
#ifdef HAS_SCHTASK
        const auto now = QDateTime::currentMSecsSinceEpoch();
        // now < s_lastQueryMs：时钟回拨同样视为过期（否则窗口会被“负差值”无限拉长）
        if (refresh || !s_hasCache || now - s_lastQueryMs > 60 * 1000 || now < s_lastQueryMs) {
            s_cachedIsOn = isOn_reg() || ScheduledTask::queryTask(SCHTASK_NAME);
            s_lastQueryMs = now;
            s_hasCache = true;
        }
        return s_cachedIsOn;
#else
        Q_UNUSED(refresh);
        return isOn_reg();
#endif
    }

    static void on() {
        bool knownResult = true; // 成功路径的终态（true）直接写回缓存，省去一次 schtasks 查询 fork
#ifdef HAS_SCHTASK
        // MS doc: 此函数是 CheckTokenMembership 的包装器, 建议直接调用该函数来确定管理员组状态; 而不是调用 IsUserAnAdmin
        if (IsUserAnAdmin()) {
            // 先建任务、成功后再移除注册表项：createTask 失败时保留原有（注册表）自启动能力，而不是两边都没有
            if (ScheduledTask::createTask(SCHTASK_NAME)) {
                off_reg();
            } else {
                knownResult = false; // 终态未知：回退为强制刷新
                QMessageBox::warning(nullptr, "Failed to create ScheduledTask", "maybe check log?(if any)");
            }
            // 这里使用`QMessageBox`而非`sysTray.showMessage`，是为了避免循环依赖 & 保持Startup独立性
        } else {
            // 但是一般认为 reg || schtasks 有一个存在就是自启动了，此时不会调用该函数，也不会进入这个if
            if (ScheduledTask::queryTask(SCHTASK_NAME)) { // no admin, can't delete
                knownResult = false; // 冲突未解决：终态未知
                QMessageBox::warning(nullptr, "Conflict: schtask vs reg",
                                     "ScheduledTask exists, but no privilege to delete. Please run as Administrator & do it again.");
            } else {
                on_reg();
            }
        }
#else
        on_reg();
#endif
        if (knownResult)
            setCacheKnown(true);
        else
            invalidateCache();
    }

    static void off() {
        off_reg();
        bool deleteOk = true; // 删除失败时终态未知（任务可能仍存在），回退为强制刷新
#ifdef HAS_SCHTASK
        if (ScheduledTask::queryTask(SCHTASK_NAME)) {
            if (!ScheduledTask::deleteTask(SCHTASK_NAME)) {
                deleteOk = false;
                bool isAdmin = IsUserAnAdmin();
                qWarning() << "Failed to delete ScheduledTask:" << SCHTASK_NAME << isAdmin;
                QMessageBox::warning(nullptr, QString("Failed to delete ScheduledTask: %1").arg(SCHTASK_NAME),
                                     isAdmin ?
                                     "em mm, something went wrong... maybe check log?(if any)" :
                                     "Please run as Administrator & do it again.");
            }
        }
#endif
        if (deleteOk)
            setCacheKnown(false); // reg 已删 + 任务已删（或本就不存在）：终态 false
        else
            invalidateCache();
    }

    static void toggle() {
        isOn() ? off() : on();
    }

private:
    static QString applicationPath() {
        return QDir::toNativeSeparators(QApplication::applicationFilePath());
    }

    /// 使 isOn() 缓存失效（写操作结果未知时调用：下次查询强制刷新重建缓存）
    static void invalidateCache() {
#ifdef HAS_SCHTASK
        isOn(true); // 强制刷新并重建缓存
#else
        // 无 schtask 时 isOn() 直查注册表，无缓存，无需处理
#endif
    }

    /// 已知终态时直接写回缓存（写操作成功路径调用，省去一次 schtasks 查询 fork）
    static void setCacheKnown(bool state) {
#ifdef HAS_SCHTASK
        s_cachedIsOn = state;
        s_lastQueryMs = QDateTime::currentMSecsSinceEpoch();
        s_hasCache = true;
#else
        Q_UNUSED(state);
#endif
    }

    static void on_reg() {
        QSettings reg(REG_AUTORUN, QSettings::NativeFormat);
        // Run 键值路径含空格时必须带引号，否则开机启动失败或参数截断
        reg.setValue(REG_APP_NAME, "\"" + applicationPath() + "\"");
    }

    static void off_reg() {
        QSettings reg(REG_AUTORUN, QSettings::NativeFormat);
        reg.remove(REG_APP_NAME);
    }

    static bool isOn_reg() {
        QSettings reg(REG_AUTORUN, QSettings::NativeFormat);
        auto appPath = applicationPath();
        auto path = reg.value(REG_APP_NAME);
        if (path.isValid()) {
            // 注册表值可能带引号，统一去除后比较；同时统一分隔符与大小写
            QString stored = path.toString();
            stored.remove('\"');
            const bool matched = stored.compare(appPath, Qt::CaseInsensitive) == 0; // 只比较一次
            if (!matched)
                qWarning() << "REG: AutoRun path mismatch:" << stored << appPath;
            return matched;
        }
        return false;
    }

private:
    // HKEY_CURRENT_USER 仅仅对当前用户有效，但不需要管理员权限
    inline static const auto REG_AUTORUN = R"(HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run)";
    // 标识符，不能重复
    inline static const auto REG_APP_NAME = "SwitchTask";
#ifdef HAS_SCHTASK
    inline static const auto SCHTASK_NAME = "SwitchTask Startup";
    // isOn() 的 60 秒缓存（schtasks 查询需 fork 子进程；写操作后经 setCacheKnown/invalidateCache 更新）
    inline static qint64 s_lastQueryMs = 0;
    inline static bool s_cachedIsOn = false;
    inline static bool s_hasCache = false;
#endif
};

#endif //WIN_SWITCHER_STARTUP_H
