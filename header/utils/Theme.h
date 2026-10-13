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
    /// 当前生效的深色与否（配置优先，其次系统）
    inline bool isDark() {
        switch (cfg.getThemeMode()) {
            case ThemeLight: return false;
            case ThemeDark: return true;
            default: break;
        }
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    /// 把配置中的强制模式同步给 Qt（影响原生控件的配色）；auto 时交还给系统
    inline void applyConfiguredScheme() {
        auto* hints = QGuiApplication::styleHints();
        switch (cfg.getThemeMode()) {
            case ThemeLight: hints->setColorScheme(Qt::ColorScheme::Light); break;
            case ThemeDark: hints->setColorScheme(Qt::ColorScheme::Dark); break;
            default: hints->setColorScheme(Qt::ColorScheme::Unknown); break; // 跟随系统
        }
    }

    // ---------- 切换器弹窗（Widget 面板）配色 ----------
    inline QColor panelFill() { return isDark() ? QColor(25, 25, 25, 100) : QColor(247, 247, 247, 190); }
    inline QColor panelStroke() { return isDark() ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 26); }
    inline QColor labelColor() { return isDark() ? QColor(240, 240, 240) : QColor(34, 34, 34); }
    inline QColor hoverFill() { return isDark() ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 18); }
    inline QColor ringColor() { return isDark() ? QColor(255, 255, 255, 140) : QColor(255, 255, 255, 200); }

    /// 品牌选中色（马卡龙·婴儿蓝）：深浅色模式保持一致，形成统一的选中语言
    inline QColor selectionFill() { return QColor(162, 210, 255, 225); }

    // ---------- 托盘菜单样式表 ----------
    /// macOS 风格：圆角 + 毛玻璃透底 + 品牌色选中胶囊 + 圆形指示点（随模式切换重新应用）
    inline QString menuStyleSheet() {
        const bool dark = isDark();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
        const QColor bg = dark ? QColor(40, 40, 40, 236) : QColor(250, 250, 250, 244);
        const QColor border = dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 26);
        const QColor text = dark ? QColor(238, 238, 238) : QColor(40, 40, 40);
        const QColor disabled = dark ? QColor(255, 255, 255, 84) : QColor(0, 0, 0, 80);
        const QColor separator = dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 22);
        const QColor selBg = QColor(162, 210, 255, 232);
        const QColor selText = QColor(18, 42, 66);
        const QColor dotRing = dark ? QColor(255, 255, 255, 90) : QColor(0, 0, 0, 70);
        const QColor dotFill = QColor(162, 210, 255);
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
