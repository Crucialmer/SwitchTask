#ifndef WIN_SWITCHER_STARTUP_H
#define WIN_SWITCHER_STARTUP_H

#include <QString>
#include <QSettings>
#include <QDir>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QMessageBox>
#include <QProcess>
#include <shlobj_core.h>

// 纯绿色/非管理员模式：始终使用当前用户注册表 Run 键，不依赖计划任务或管理员权限

class Startup {
public:
    Startup() = delete;
    friend class SystemTray;

    /// 状态查询：当前用户注册表 Run 值（结合 StartupApproved 禁用标记判定）
    static bool isOn(bool /*refresh*/ = false) {
        return isOn_reg() && !isStartupDisabled();
    }

    /// 开启自启动：写入当前用户注册表 Run 键（标准用户权限，无需管理员提权）
    static void on() {
        on_reg();
        // 清除“任务管理器 > 启动”的禁用覆盖标记（StartupApproved\Run）
        clearStartupDisabled();
    }

    /// 关闭自启动：移除当前用户注册表 Run 键与禁用标记
    static void off() {
        off_reg();
        clearStartupDisabled();
        // 若系统曾残留管理员模式的计划任务，顺便尝试静默清理（零临时文件、不调用 PowerShell）
        QProcess::execute("schtasks", {"/delete", "/tn", SCHTASK_NAME, "/f"});
    }

    static void toggle() {
        isOn() ? off() : on();
    }

private:
    static QString applicationPath() {
        return QDir::toNativeSeparators(QApplication::applicationFilePath());
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
    inline static const auto SCHTASK_NAME = "SwitchTask Startup";
};

#endif //WIN_SWITCHER_STARTUP_H
