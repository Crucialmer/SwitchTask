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
    /// macOS 风格：平滑圆角 + 通透毛玻璃 + 原生 Checkmark 指示器 + 优雅悬停胶囊（随模式切换即时重绘）
    inline QString menuStyleSheet() {
        const bool dark = isDark();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
        // macOS 风格：通透毛玻璃底色（约 78%~80% alpha，配合 SetWindowCompositionAttribute 呈现通透的环境磨砂光影）
        const QColor bg = dark ? QColor(32, 32, 36, 198) : QColor(248, 248, 250, 204);
        const QColor border = dark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 26);
        const QColor text = dark ? QColor(242, 242, 245) : QColor(28, 28, 30);
        const QColor disabled = dark ? QColor(255, 255, 255, 75) : QColor(0, 0, 0, 70);
        const QColor separator = dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 20);

        // 选中悬停态（macOS 菜单高亮胶囊）：
        // 深色模式：微光白半透明胶囊 + 纯白高亮字；
        // 浅色模式：经典品牌马卡龙·婴儿蓝胶囊 + 高对比度深蓝文字
        const QColor selBg = dark ? QColor(255, 255, 255, 34) : brandColor(220);
        const QColor selText = dark ? QColor(255, 255, 255) : QColor(10, 30, 56);
        const QString checkIcon = dark ? QStringLiteral(":/img/checkmark-dark.svg") : QStringLiteral(":/img/checkmark-light.svg");

        return QStringLiteral(
                   "QMenu {"
                   "  background-color: %1;"
                   "  border: 1px solid %2;"
                   "  border-radius: 10px;"
                   "  padding: 5px;"
                   "  font-family: 'Segoe UI Variable Text', 'Segoe UI', -apple-system, sans-serif;"
                   "  font-size: 12px;"
                   "}"
                   "QMenu::item {"
                   "  padding: 5px 22px 5px 26px;"
                   "  border-radius: 5px;"
                   "  margin: 1px 2px;"
                   "  color: %3;"
                   "  background: transparent;"
                   "}"
                   "QMenu::item:selected { background-color: %4; color: %5; }"
                   "QMenu::item:disabled { color: %6; }"
                   "QMenu::separator { height: 1px; background: %7; margin: 4px 8px; }"
                   "QMenu::indicator { width: 12px; height: 12px; margin-left: 7px; }"
                   "QMenu::indicator:checked { image: url(%8); }"
                   "QMenu::indicator:unchecked { image: none; border: none; background: transparent; }"
                   "QMenu::right-arrow { image: url(:/img/chevron-right.svg); width: 11px; height: 11px; margin-right: 5px; }")
            .arg(css(bg))
            .arg(css(border))
            .arg(css(text))
            .arg(css(selBg))
            .arg(css(selText))
            .arg(css(disabled))
            .arg(css(separator))
            .arg(checkIcon);
    }
} // namespace Theme

#endif //WIN_SWITCHER_THEME_H
