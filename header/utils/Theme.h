#ifndef WIN_SWITCHER_THEME_H
#define WIN_SWITCHER_THEME_H

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>
#include "ConfigManager.h"

/// 深/浅色主题：默认跟随系统（Windows 应用模式）自动切换；
/// 可在 config.ini 中写 `ThemeMode = light / dark / auto`（或 0/1/2）强制指定。
/// 所有自定义绘制的配色统一从这里取，保证弹窗、托盘菜单、更新窗口风格一致。
namespace Theme {
    // ---------- 基础色常量（同一色值只定义一处；alpha 的各自微差在调用点表达） ----------
    constexpr int BrandR = 162, BrandG = 210, BrandB = 255; // 品牌色（马卡龙·婴儿蓝 #A2D2FF）
    constexpr int LightSurfaceR = 251, LightSurfaceG = 251, LightSurfaceB = 251; // 浅色面板底色基值
    inline QColor brandColor(int alpha) { return QColor(BrandR, BrandG, BrandB, alpha); }
    inline QColor lightSurface(int alpha) { return QColor(LightSurfaceR, LightSurfaceG, LightSurfaceB, alpha); }

    /// 当前生效的深色与否（配置优先，其次系统）
    inline bool isDark() {
        switch (CFG.getThemeMode()) {
            case ThemeLight: return false;
            case ThemeDark: return true;
            default: break;
        }
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    /// 把配置中的强制模式同步给 Qt（影响原生控件的配色）；auto 时交还给系统
    inline void applyConfiguredScheme() {
        auto* hints = QGuiApplication::styleHints();
        switch (CFG.getThemeMode()) {
            case ThemeLight: hints->setColorScheme(Qt::ColorScheme::Light); break;
            case ThemeDark: hints->setColorScheme(Qt::ColorScheme::Dark); break;
            default: hints->setColorScheme(Qt::ColorScheme::Unknown); break; // 跟随系统
        }
    }

    // ---------- 切换器弹窗（Widget 面板）配色 ----------
    // macOS 式「深色通透 HUD」：切换器面板采用深石墨半透明质感，配合 Win11 硬件加速瞬态亚克力
    // ——深底 + 浅色文字 + 细微受光缘。透明度与色彩经过精确微调，兼顾对比度与透气感
    inline QColor panelFill() { return QColor(28, 28, 32, 205); }
    inline QColor panelStroke() { return QColor(255, 255, 255, 38); }
    inline QColor labelColor() { return QColor(245, 245, 247); }
    inline QColor hoverFill() { return QColor(255, 255, 255, 25); }

    /// 选中底（经典马卡龙·婴儿蓝）：#A2D2FF（α240）——柔和醒目，在深色 HUD 上圆润饱满、零硬描环
    inline QColor selectionFill() { return QColor(162, 210, 255, 240); }
    inline QColor selectionStroke() { return QColor(255, 255, 255, 60); }

    // ---------- 面板背景材质（Apple 式细化：光影/受光缘）----------
    /// 光影渐变：顶部微提亮（环境漫反射层）
    inline QColor panelGradTop() { return QColor(255, 255, 255, 18); }
    /// 光影渐变：底部沉降
    inline QColor panelGradBottom() { return QColor(0, 0, 0, 26); }
    /// 顶部内侧受光缘（细高光线，随平滑圆角自然淡出）
    inline QColor panelTopSheen() { return QColor(255, 255, 255, 65); }

    // ---------- 更新窗口配色 ----------
    /// 更新说明区底色（QSS 需设实底，圆角才生效）
    inline QColor dialogSurface() { return isDark() ? QColor(0x2D, 0x2D, 0x2D) : QColor(0xF6, 0xF6, 0xF6); }
    /// 次级文字（「Current Version」等；深色下提亮保证可读，浅色保持原观感）
    inline QColor dialogSecondaryText() { return isDark() ? QColor(0x9E, 0x9E, 0x9E) : QColor(0x7A, 0x7A, 0x7A); }

    // ---------- 托盘菜单样式表 ----------
    /// macOS 风格：平滑圆角 + 通透毛玻璃 + 原生 Checkmark 指示器 + 优雅悬停胶囊 + 顶部品牌卡片与加粗排版（随模式切换即时重绘）
    inline QString menuStyleSheet() {
        const bool dark = isDark();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
        // macOS 风格：通透毛玻璃底色（约 78%~80% alpha，配合 SetWindowCompositionAttribute 呈现通透的环境磨砂光影）
        const QColor bg = dark ? QColor(28, 28, 32, 198) : QColor(248, 248, 250, 204);
        const QColor border = dark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 26);
        const QColor text = dark ? QColor(242, 242, 245) : QColor(28, 28, 30);
        const QColor disabled = dark ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 110);
        const QColor separator = dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 18);
        const QColor badgeBg = dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 14);

        // 选中悬停态（macOS 菜单高亮胶囊）：
        // 深色模式：微光白半透明胶囊 + 纯白高亮字；
        // 浅色模式：Apple 原厂 Accent Blue (#007AFF) + 纯白高亮字
        const QColor selBg = dark ? QColor(255, 255, 255, 34) : QColor(0, 122, 255);
        const QColor selText = QColor(255, 255, 255);
        const QString checkIcon = dark ? QStringLiteral(":/img/checkmark-dark.svg") : QStringLiteral(":/img/checkmark-light.svg");

        // 字体族：默认简体中文 Windows 系统默认字体 “Microsoft YaHei UI”（微软雅黑），兼顾备用字体；
        // 同时支持在 config.ini [menu] 节通过 font_family 自由定制
        const auto customFontFamily = CFG.get("menu/font_family", "").toString().trimmed();
        QString fontFamily;
        if (!customFontFamily.isEmpty()) {
            if (customFontFamily.contains('\'') || customFontFamily.contains('"') || customFontFamily.contains(',')) {
                fontFamily = customFontFamily + QStringLiteral(", 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif");
            } else {
                fontFamily = QStringLiteral("'%1', 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif").arg(customFontFamily);
            }
        } else {
            fontFamily = QStringLiteral("'Microsoft YaHei UI', 'Microsoft YaHei', 'Segoe UI Variable Text', 'Segoe UI', -apple-system, sans-serif");
        }

        // 字体加粗与协调：默认 bold (700) 饱满扎实，同时支持在 config.ini [menu] 节通过 font_weight / font_size 自由定制
        const auto weightRaw = CFG.get("menu/font_weight", "bold").toString().trimmed().toLower();
        int weight = 700; // 默认 bold (700)
        if (weightRaw == "bold" || weightRaw == "b" || weightRaw == "700") weight = 700;
        else if (weightRaw == "semibold" || weightRaw == "demibold" || weightRaw == "600") weight = 600;
        else if (weightRaw == "medium" || weightRaw == "500") weight = 500;
        else if (weightRaw == "normal" || weightRaw == "regular" || weightRaw == "400") weight = 400;
        else if (auto val = weightRaw.toInt(); val >= 100 && val <= 900) weight = val;

        auto fontSize = CFG.get("menu/font_size", 13.0).toDouble();
        if (fontSize <= 0) fontSize = 13.0;

        QString qss = QStringLiteral(
                   "QMenu {"
                   "  background-color: %1;"
                   "  border: 1px solid %2;"
                   "  border-radius: 10px;"
                   "  padding: 5px;"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: %3px;"
                   "  font-weight: %4;"
                   "}"
                   "#menuHeaderWidget { background: transparent; }"
                   "#menuHeaderTitle {"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: %3px;"
                   "  font-weight: %4;"
                   "  color: %5;"
                   "  background: transparent;"
                   "}"
                   "#menuHeaderVersion {"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: 11px;"
                   "  font-weight: 500;"
                   "  color: %6;"
                   "  background-color: %7;"
                   "  border-radius: 4px;"
                   "  padding: 1px 6px;"
                   "}"
                   "QMenu::item {"
                   "  padding: 6px 22px 6px 26px;"
                   "  border-radius: 5px;"
                   "  margin: 1px 2px;"
                   "  color: %5;"
                   "  background: transparent;"
                   "  font-weight: %4;"
                   "}"
                   "QMenu::item:selected { background-color: %8; color: %9; }"
                   "QMenu::item:disabled { color: %6; }"
                   "QMenu::separator { height: 1px; background: @SEP@; margin: 4px 8px; }"
                   "QMenu::indicator { width: 12px; height: 12px; margin-left: 7px; }"
                   "QMenu::indicator:checked { image: url(@CHK@); }"
                   "QMenu::indicator:checked:selected { image: url(:/img/checkmark-dark.svg); }"
                   "QMenu::indicator:unchecked { image: none; border: none; background: transparent; }"
                   "QMenu::right-arrow { image: url(:/img/chevron-right.svg); width: 11px; height: 11px; margin-right: 5px; }")
            .arg(css(bg), css(border), QString::number(fontSize), QString::number(weight), css(text), css(disabled), css(badgeBg), css(selBg), css(selText));
        qss.replace("@SEP@", css(separator));
        qss.replace("@CHK@", checkIcon);
        qss.replace("@FONT_FAMILY@", fontFamily);
        return qss;
    }
} // namespace Theme

#endif //WIN_SWITCHER_THEME_H
