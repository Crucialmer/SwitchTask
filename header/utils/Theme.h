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
    // 浅色填充取近乎不透明：半透明浅色在部分合成环境（透明窗口叠黑底）下会被压成脏灰，
    // 近乎不透明可保证面板在任何壁纸/窗口背景上都呈现干净浅色（深色态保持原有通透观感）
    inline QColor panelFill() { return isDark() ? QColor(25, 25, 25, 100) : lightSurface(252); }
    inline QColor panelStroke() { return isDark() ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 26); }
    inline QColor labelColor() { return isDark() ? QColor(240, 240, 240) : QColor(34, 34, 34); }
    inline QColor hoverFill() { return isDark() ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 18); }
    inline QColor ringColor() { return isDark() ? QColor(255, 255, 255, 140) : QColor(255, 255, 255, 200); }

    /// 品牌选中色（马卡龙·婴儿蓝）：深浅色模式保持一致，形成统一的选中语言
    inline QColor selectionFill() { return brandColor(225); }

    // ---------- 更新窗口配色 ----------
    /// 更新说明区底色（QSS 需设实底，圆角才生效）
    inline QColor dialogSurface() { return isDark() ? QColor(0x2D, 0x2D, 0x2D) : QColor(0xF6, 0xF6, 0xF6); }
    /// 次级文字（「Current Version」等；深色下提亮保证可读，浅色保持原观感）
    inline QColor dialogSecondaryText() { return isDark() ? QColor(0x9E, 0x9E, 0x9E) : QColor(0x7A, 0x7A, 0x7A); }

    // ---------- 托盘菜单样式表 ----------
    /// macOS 风格：圆角 + 毛玻璃透底 + 品牌色选中胶囊 + 圆形指示点（随模式切换重新应用）
    /// 注：右箭头使用 svg 的固定次级灰 #8E8E93（两种主题一致，属有意的次级墨色；菜单文字则随主题）
    inline QString menuStyleSheet() {
        const bool dark = isDark();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
        const QColor bg = dark ? QColor(40, 40, 40, 236) : lightSurface(250);
        const QColor border = dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 26);
        const QColor text = dark ? QColor(238, 238, 238) : QColor(40, 40, 40);
        const QColor disabled = dark ? QColor(255, 255, 255, 84) : QColor(0, 0, 0, 80);
        const QColor separator = dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 22);
        const QColor selBg = brandColor(232);
        const QColor selText = QColor(18, 42, 66);
        const QColor dotRing = dark ? QColor(255, 255, 255, 90) : QColor(0, 0, 0, 70);
        const QColor dotFill = brandColor(255);
        return QStringLiteral(
                   "QMenu {"
                   "  background-color: %1;" // 面板底色（毛玻璃之上的一层着色）
                   "  border: 1px solid %2;"
                   "  border-radius: 10px;"
                   "  padding: 6px;"
                   "}"
                   "QMenu::item {"
                   "  padding: 7px 30px 7px 34px;" // 左侧预留指示点栏位
                   "  border-radius: 7px;"
                   "  margin: 1px 2px;"
                   "  color: %3;"
                   "  background: transparent;"
                   "}"
                   "QMenu::item:selected { background-color: %4; color: %5; }"
                   "QMenu::item:disabled { color: %6; }"
                   "QMenu::separator { height: 1px; background: %7; margin: 5px 10px; }"
                   "QMenu::indicator { width: 14px; height: 14px; margin-left: 10px; }"
                   "QMenu::indicator:checked { background-color: %8; border-radius: 7px; }"
                   "QMenu::indicator:unchecked { border: 1px solid %9; border-radius: 7px; background: transparent; }"
                   "QMenu::right-arrow { image: url(:/img/chevron-right.svg); width: 12px; height: 12px; margin-right: 6px; }")
            .arg(css(bg))
            .arg(css(border))
            .arg(css(text))
            .arg(css(selBg))
            .arg(css(selText))
            .arg(css(disabled))
            .arg(css(separator))
            .arg(css(dotFill))
            .arg(css(dotRing));
    }
} // namespace Theme

#endif //WIN_SWITCHER_THEME_H
