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
        ensureCache();
        return m_cachedMonitor;
    }

    void setDisplayMonitor(DisplayMonitor monitor) {
        set("DisplayMonitor", monitor);
    }

    /// 托盘右键弹出窗口主题：`ThemeMode = auto / light / dark`（也可写 0/1/2；非法值回退 auto 并告警）
    ThemeMode getThemeMode() {
        ensureCache();
        return m_cachedTheme;
    }

    void setThemeMode(ThemeMode mode) {
        QString val = QStringLiteral("auto");
        if (mode == ThemeLight) val = QStringLiteral("light");
        else if (mode == ThemeDark) val = QStringLiteral("dark");
        set("ThemeMode", val);
    }

    /// 弹出任务窗口颜色：`SwitcherThemeMode = dark / light / auto`（默认深色 dark）
    ThemeMode getSwitcherThemeMode() {
        ensureCache();
        return m_cachedSwitcherTheme;
    }

    void setSwitcherThemeMode(ThemeMode mode) {
        QString val = QStringLiteral("dark");
        if (mode == ThemeLight) val = QStringLiteral("light");
        else if (mode == ThemeAuto) val = QStringLiteral("auto");
        set("SwitcherThemeMode", val);
    }

    /// 实时缩略图模式：`thumbnail/mode = integrated / popup / disabled`（以方案2舞台集成为默认设置）
    ThumbnailMode getThumbnailMode() {
        ensureCache();
        return m_cachedThumbMode;
    }

    void setThumbnailMode(ThumbnailMode mode) {
        QString val = QStringLiteral("integrated");
        if (mode == ThumbnailPopup) val = QStringLiteral("popup");
        else if (mode == ThumbnailDisabled) val = QStringLiteral("disabled");
        set("thumbnail/mode", val);
    }

protected:
    void onConfigMutated() override {
        m_cacheValid = false;
    }

private:
    explicit ConfigManager(const QString& filename) : ConfigManagerBase(filename) {}

    void ensureCache() {
        if (m_cacheValid) return;
        m_cacheValid = true;

        int monitor = get("DisplayMonitor", DisplayMonitor::MouseMonitor).toInt();
        if (monitor < 0 || monitor >= DisplayMonitor::EnumCount) {
            qWarning() << "Invalid DisplayMonitor enum" << monitor;
            monitor = DisplayMonitor::MouseMonitor;
        }
        m_cachedMonitor = static_cast<DisplayMonitor>(monitor);

        const auto themeRaw = get("ThemeMode", QStringLiteral("auto")).toString().trimmed().toLower();
        if (themeRaw == "light" || themeRaw == "1")
            m_cachedTheme = ThemeLight;
        else if (themeRaw == "dark" || themeRaw == "2")
            m_cachedTheme = ThemeDark;
        else {
            if (!(themeRaw == "auto" || themeRaw == "0" || themeRaw.isEmpty()))
                qWarning() << "Invalid ThemeMode (expect auto/light/dark), fallback to auto:" << themeRaw;
            m_cachedTheme = ThemeAuto;
        }

        const auto switcherThemeRaw = get("SwitcherThemeMode", QStringLiteral("dark")).toString().trimmed().toLower();
        if (switcherThemeRaw == "light" || switcherThemeRaw == "1")
            m_cachedSwitcherTheme = ThemeLight;
        else if (switcherThemeRaw == "auto" || switcherThemeRaw == "0")
            m_cachedSwitcherTheme = ThemeAuto;
        else {
            if (!(switcherThemeRaw == "dark" || switcherThemeRaw == "2" || switcherThemeRaw.isEmpty()))
                qWarning() << "Invalid SwitcherThemeMode (expect dark/light/auto), fallback to dark:" << switcherThemeRaw;
            m_cachedSwitcherTheme = ThemeDark;
        }

        const auto thumbRaw = get("thumbnail/mode", QStringLiteral("integrated")).toString().trimmed().toLower();
        if (thumbRaw == "popup" || thumbRaw == "floating" || thumbRaw == "1")
            m_cachedThumbMode = ThumbnailPopup;
        else if (thumbRaw == "disabled" || thumbRaw == "none" || thumbRaw == "off" || thumbRaw == "2")
            m_cachedThumbMode = ThumbnailDisabled;
        else {
            if (!(thumbRaw == "integrated" || thumbRaw == "stage" || thumbRaw == "0" || thumbRaw.isEmpty()))
                qWarning() << "Invalid thumbnail/mode (expect integrated/popup/disabled), fallback to integrated:" << thumbRaw;
            m_cachedThumbMode = ThumbnailIntegrated;
        }
    }

    bool m_cacheValid = false;
    DisplayMonitor m_cachedMonitor = DisplayMonitor::MouseMonitor;
    ThemeMode m_cachedTheme = ThemeAuto;
    ThemeMode m_cachedSwitcherTheme = ThemeDark;
    ThumbnailMode m_cachedThumbMode = ThumbnailIntegrated;
};

#endif //WIN_SWITCHER_CONFIGMANAGER_H
