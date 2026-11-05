#ifndef WIN_SWITCHER_CONFIGMANAGER_H
#define WIN_SWITCHER_CONFIGMANAGER_H

#include <QSettings>
#include <QApplication>
#include <QDir>
#include "ConfigManagerBase.h"

// 注意：对于大量使用的类，header-only 模式会导致编译时间过长
#define CFG ConfigManager::instance()

enum DisplayMonitor {
    PrimaryMonitor, // 0 主显示器
    MouseMonitor, // 1 跟随鼠标
    EnumCount // Just for count
};

enum ThemeMode {
    ThemeAuto, // 0 跟随系统深浅色（默认）
    ThemeLight, // 1 强制浅色
    ThemeDark, // 2 强制深色
};

enum ThumbnailMode {
    ThumbnailIntegrated, // 0: 舞台集成预览 (默认推荐)
    ThumbnailPopup,      // 1: 悬浮卡片预览
    ThumbnailDisabled,   // 2: 关闭实时缩略图
    ThumbnailModeCount
};

class ConfigManager : public ConfigManagerBase {
    inline static const QString FileName = "config.ini";

public:
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    static ConfigManager& instance() {
        // 实时绿色运行模式：配置始终严格保存在程序自身目录下，绝不向 AppData 写入残留文件
        static const auto filePath = QApplication::applicationDirPath() + "/" + FileName;
        static ConfigManager instance{filePath}; // multiple threads safe
        return instance;
    }

public:
    DisplayMonitor getDisplayMonitor() {
        auto monitor = get("DisplayMonitor", DisplayMonitor::MouseMonitor).toInt();
        if (monitor < 0 || monitor >= DisplayMonitor::EnumCount) {
            qWarning() << "Invalid DisplayMonitor enum" << monitor;
            monitor = DisplayMonitor::MouseMonitor;
        }
        return static_cast<DisplayMonitor>(monitor);
    }

    void setDisplayMonitor(DisplayMonitor monitor) {
        set("DisplayMonitor", monitor);
    }

    /// 深浅色模式：`ThemeMode = auto / light / dark`（也可写 0/1/2；非法值回退 auto 并告警）
    ThemeMode getThemeMode() {
        const auto raw = get("ThemeMode", QStringLiteral("auto")).toString().trimmed().toLower();
        if (raw == "light" || raw == "1")
            return ThemeLight;
        if (raw == "dark" || raw == "2")
            return ThemeDark;
        if (!(raw == "auto" || raw == "0" || raw.isEmpty()))
            qWarning() << "Invalid ThemeMode (expect auto/light/dark), fallback to auto:" << raw;
        return ThemeAuto;
    }

    /// 实时缩略图模式：`thumbnail/mode = integrated / popup / disabled`（以方案2舞台集成为默认设置）
    ThumbnailMode getThumbnailMode() {
        const auto raw = get("thumbnail/mode", QStringLiteral("integrated")).toString().trimmed().toLower();
        if (raw == "popup" || raw == "floating" || raw == "1")
            return ThumbnailPopup;
        if (raw == "disabled" || raw == "none" || raw == "off" || raw == "2")
            return ThumbnailDisabled;
        if (raw == "integrated" || raw == "stage" || raw == "0" || raw.isEmpty())
            return ThumbnailIntegrated;
        qWarning() << "Invalid thumbnail/mode (expect integrated/popup/disabled), fallback to integrated:" << raw;
        return ThumbnailIntegrated;
    }

    void setThumbnailMode(ThumbnailMode mode) {
        QString val = QStringLiteral("integrated");
        if (mode == ThumbnailPopup) val = QStringLiteral("popup");
        else if (mode == ThumbnailDisabled) val = QStringLiteral("disabled");
        set("thumbnail/mode", val);
    }

private:
    explicit ConfigManager(const QString& filename) : ConfigManagerBase(filename) {}
};

#endif //WIN_SWITCHER_CONFIGMANAGER_H
