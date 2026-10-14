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

    /// 状态查询：schtasks 分量带缓存、注册表两项实时读（见下方缓存策略）。
    /// 缓存策略：schtasks 结果缓存 60 秒（fork 子进程，托盘菜单弹出不应频繁等待）；
    /// Run 值位与 StartupApproved 禁用标记为 µs 级注册表读取、且恰是外部工具会改的两项——实时读不吃缓存。
    /// 状态可能被本程序、任务计划程序或系统优化工具改动：本程序写操作后按已知终态写回 schtasks 分量
    /// （setSchtaskCache）或立即刷新（invalidateCache）。
    static bool isOn(bool refresh = false) {
#ifdef HAS_SCHTASK
        const auto now = QDateTime::currentMSecsSinceEpoch();
        // now < s_lastQueryMs：时钟回拨同样视为过期（否则窗口会被“负差值”无限拉长）
        if (refresh || !s_hasCache || now - s_lastQueryMs > 60 * 1000 || now < s_lastQueryMs) {
            s_cachedSchtask = ScheduledTask::queryTask(SCHTASK_NAME); // 60s 缓存仅覆盖 schtasks（fork 子进程）
            s_lastQueryMs = now;
            s_hasCache = true;
        }
        // 被“任务管理器 > 启动”禁用时（StartupApproved 覆盖标记），Run 值存在也不算自启动
        return (isOn_reg() || s_cachedSchtask) && !isStartupDisabled();
#else
        Q_UNUSED(refresh);
        return isOn_reg() && !isStartupDisabled();
#endif
    }

    static void on() {
#ifdef HAS_SCHTASK
        // MS doc: 此函数是 CheckTokenMembership 的包装器, 建议直接调用该函数来确定管理员组状态; 而不是调用 IsUserAnAdmin
        if (IsUserAnAdmin()) {
            // 先建任务、成功后再移除注册表项：createTask 失败时保留原有（注册表）自启动能力，而不是两边都没有
            if (ScheduledTask::createTask(SCHTASK_NAME)) {
                off_reg();
                setSchtaskCache(true); // schtasks 分量终态：已建
            } else {
                invalidateCache(); // 终态未知：回退为强制刷新
                QMessageBox::warning(nullptr, "Failed to create ScheduledTask", "maybe check log?(if any)");
            }
            // 这里使用`QMessageBox`而非`sysTray.showMessage`，是为了避免循环依赖 & 保持Startup独立性
        } else {
            // 但是一般认为 reg || schtasks 有一个存在就是自启动了，此时不会调用该函数，也不会进入这个if
            if (ScheduledTask::queryTask(SCHTASK_NAME)) { // no admin, can't delete
                invalidateCache(); // 冲突未解决：终态未知
                QMessageBox::warning(nullptr, "Conflict: schtask vs reg",
                                     "ScheduledTask exists, but no privilege to delete. Please run as Administrator & do it again.");
            } else {
                on_reg();
                setSchtaskCache(false); // schtasks 分量终态：查询已确认不存在
            }
        }
#else
        on_reg();
#endif
        // 写路径第二步：清除“任务管理器 > 启动”的禁用覆盖标记（StartupApproved\Run）。
        // 只写 Run 值不清标记时，登录仍不会自启、界面却显示 ON——两步都成功才算成功。
        if (!clearStartupDisabled()) {
            qWarning() << "Failed to clear StartupApproved\\Run marker";
            invalidateCache(); // 标记清除失败：终态未知
            QMessageBox::warning(nullptr, "Failed to re-enable startup",
                                 "Windows still has this startup entry disabled (Task Manager > Startup apps). "
                                 "Please enable it there, or run as Administrator.");
        }
    }

    static void off() {
        off_reg();
        clearStartupDisabled(); // 同时清除禁用覆盖标记：避免“残留 03 标记 + 之后 on() 被静默阻止”的陷阱
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
            setSchtaskCache(false); // reg 已删 + 任务已删（或本就不存在）：schtasks 分量终态 false
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

    /// 已知 schtasks 分量终态时直接写回缓存（写操作成功路径调用，省去一次 schtasks 查询 fork）
    static void setSchtaskCache(bool exists) {
#ifdef HAS_SCHTASK
        s_cachedSchtask = exists;
        s_lastQueryMs = QDateTime::currentMSecsSinceEpoch();
        s_hasCache = true;
#else
        Q_UNUSED(exists);
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

    /// “任务管理器/设置 > 启动” 的禁用覆盖标记（StartupApproved\Run）：
    /// Run 值存在 ≠ 会自启——被禁用时系统写入同名 12 字节 REG_BINARY（byte0 奇数=禁用 03/07/09、
    /// 偶数=启用 02/06/08）；值缺失 = 无覆盖 = 启用。08/09 为灰化（策略锁定）态。计划任务不在此机制内。
    static bool isStartupDisabled() {
        BYTE buf[64] = {};
        DWORD type = 0, size = sizeof(buf);
        if (RegGetValueW(HKEY_CURRENT_USER, STARTUP_APPROVED_RUN, REG_APP_NAME_W, RRF_RT_REG_BINARY, &type, buf, &size) != ERROR_SUCCESS)
            return false; // 无该值（或类型不符）= 无覆盖 = 启用
        return size >= 1 && (buf[0] & 0x01) != 0; // 奇数 = 禁用
    }

    /// 清除禁用覆盖标记（回到“无覆盖 = 启用”的干净态）；从未被禁用过则不写任何值（注册表零污染，删除法）
    static bool clearStartupDisabled() {
        BYTE buf[16] = {};
        DWORD type = 0, size = sizeof(buf);
        if (RegGetValueW(HKEY_CURRENT_USER, STARTUP_APPROVED_RUN, REG_APP_NAME_W, RRF_RT_REG_BINARY, &type, buf, &size) != ERROR_SUCCESS)
            return true; // 无标记：目标态已达成
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_APPROVED_RUN, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
            return false;
        const auto del = RegDeleteValueW(key, REG_APP_NAME_W);
        RegCloseKey(key);
        return del == ERROR_SUCCESS || del == ERROR_FILE_NOT_FOUND;
    }

private:
    // HKEY_CURRENT_USER 仅仅对当前用户有效，但不需要管理员权限
    inline static const auto REG_AUTORUN = R"(HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run)";
    // 标识符，不能重复
    inline static const auto REG_APP_NAME = "SwitchTask";
    inline static constexpr auto REG_APP_NAME_W = L"SwitchTask";
    // StartupApproved\Run：任务管理器/设置 的禁用覆盖标记所在键（值名 = REG_APP_NAME）
    inline static constexpr auto STARTUP_APPROVED_RUN =
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\Run";
#ifdef HAS_SCHTASK
    inline static const auto SCHTASK_NAME = "SwitchTask Startup";
    // schtasks 分量的 60 秒缓存（fork 子进程；写操作后经 setSchtaskCache/invalidateCache 更新）
    inline static qint64 s_lastQueryMs = 0;
    inline static bool s_cachedSchtask = false;
    inline static bool s_hasCache = false;
#endif
};

#endif //WIN_SWITCHER_STARTUP_H
