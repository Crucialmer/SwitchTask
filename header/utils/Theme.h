#ifndef WIN_SWITCHER_THEME_H
#define WIN_SWITCHER_THEME_H

#include <QColor>
#include <QGuiApplication>
#include <QRectF>
#include <QStyleHints>
#include <QString>
#include "ConfigManager.h"

class QPainter;

/// macOS 26 (Tahoe / Liquid Glass) 主题引擎：
/// - 弹出任务窗口颜色（`SwitcherThemeMode = dark / light / auto`，默认深色 `dark`）：作用于 Alt+Tab 切换器与悬浮预览卡片；
/// - 界面外观主题（`ThemeMode = auto / light / dark`，默认跟随系统 `auto`）：仅作用于系统托盘右键弹出窗口；
/// - 设置窗口、更新窗口与悬浮通知气泡自然跟随 Windows 系统深浅色模式。
namespace Theme {
    // ---------- 基础色常量（同一色值只定义一处；alpha 的各自微差在调用点表达） ----------
    constexpr int BrandR = 162, BrandG = 210, BrandB = 255; // 品牌色（马卡龙·婴儿蓝 #A2D2FF）
    constexpr int LightSurfaceR = 246, LightSurfaceG = 248, LightSurfaceB = 252; // 浅色 Liquid Glass 基值
    inline QColor brandColor(int alpha) { return QColor(BrandR, BrandG, BrandB, alpha); }
    inline QColor lightSurface(int alpha) { return QColor(LightSurfaceR, LightSurfaceG, LightSurfaceB, alpha); }

    /// 系统真实深浅色状态（不覆写 Qt 全局 colorScheme，确保系统切换始终精准感知）
    inline bool isSystemDark() {
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    }

    /// 通用窗口（设置/更新/气泡通知）深浅色：跟随 Windows 系统主题
    inline bool isDark() {
        return isSystemDark();
    }

    /// 系统托盘右键弹出窗口深浅色：由「界面外观主题」（ThemeMode）独立控制，仅作用于托盘右键菜单
    inline bool isMenuDark() {
        switch (CFG.getThemeMode()) {
            case ThemeLight: return false;
            case ThemeDark: return true;
            default: return isSystemDark();
        }
    }

    /// 弹出任务窗口（Widget 与 ThumbnailPopupWidget）深浅色：由「弹出任务窗口颜色」（SwitcherThemeMode）独立控制，默认深色
    inline bool isSwitcherDark() {
        switch (CFG.getSwitcherThemeMode()) {
            case ThemeLight: return false;
            case ThemeDark: return true;
            default: return isSystemDark();
        }
    }

    /// 保持 Qt 全局配色跟随操作系统（ThemeMode 仅作用于托盘右键菜单，不再篡改系统级 colorScheme）
    inline void applyConfiguredScheme() {
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    }

    /// 系统「透明效果」可用性（缓存并在 refreshSystemVisualSettings 时刷新；关闭时自动回退高对比实体表面）
    bool isTransparencyEnabled();
    void refreshSystemVisualSettings();

    // ---------- 弹出任务窗口（Widget / ThumbnailPopupWidget）macOS 26 Liquid Glass 配色 ----------
    inline QColor panelFill(bool dark, bool transparent) {
        if (dark)
            return transparent ? QColor(24, 26, 32, 196) : QColor(30, 32, 38, 248);
        return transparent ? QColor(246, 248, 252, 206) : QColor(244, 246, 250, 250);
    }
    inline QColor panelFill() { return panelFill(isSwitcherDark(), isTransparencyEnabled()); }
    inline QColor panelStroke() { return isSwitcherDark() ? QColor(255, 255, 255, 48) : QColor(0, 0, 0, 28); }
    inline QColor labelColor() { return isSwitcherDark() ? QColor(245, 245, 248) : QColor(24, 24, 28); }

    /// 悬停态：Liquid Glass 轻磨砂胶囊 + 细微边缘光
    inline QColor hoverFill() { return isSwitcherDark() ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 14); }
    inline QColor hoverStroke() { return isSwitcherDark() ? QColor(255, 255, 255, 34) : QColor(255, 255, 255, 130); }

    /// 选中态：macOS 26 Liquid Glass 聚光透镜（延续经典马卡龙·婴儿蓝基因，赋予玻璃渐变与高光折射）
    inline QColor selectionFill() { return isSwitcherDark() ? QColor(162, 210, 255, 224) : QColor(126, 192, 255, 212); }
    inline QColor selectionFillTop() { return isSwitcherDark() ? QColor(182, 222, 255, 228) : QColor(152, 206, 255, 218); }
    inline QColor selectionFillBottom() { return isSwitcherDark() ? QColor(134, 192, 255, 192) : QColor(102, 174, 255, 186); }
    inline QColor selectionStroke() { return isSwitcherDark() ? QColor(255, 255, 255, 105) : QColor(0, 122, 255, 82); }
    inline QColor selectionInnerHighlight() { return isSwitcherDark() ? QColor(255, 255, 255, 120) : QColor(255, 255, 255, 175); }

    // ---------- 舞台内凹光学井配色 ----------
    inline QColor stageEmptyText(bool dark) { return dark ? QColor(255, 255, 255, 145) : QColor(28, 28, 32, 140); }
    inline QColor stageEmptyText() { return stageEmptyText(isSwitcherDark()); }

    // ---------- 面板背景材质（保留兼容访问器） ----------
    inline QColor panelGradTop() { return isSwitcherDark() ? QColor(255, 255, 255, 22) : QColor(255, 255, 255, 95); }
    inline QColor panelGradBottom() { return isSwitcherDark() ? QColor(0, 0, 0, 30) : QColor(180, 192, 212, 30); }
    inline QColor panelTopSheen() { return isSwitcherDark() ? QColor(255, 255, 255, 78) : QColor(255, 255, 255, 215); }

    // ---------- 更新窗口配色 ----------
    inline QColor dialogSurface() { return isDark() ? QColor(0x2D, 0x2D, 0x2D) : QColor(0xF6, 0xF6, 0xF6); }
    inline QColor dialogSecondaryText() { return isDark() ? QColor(0x9E, 0x9E, 0x9E) : QColor(0x7A, 0x7A, 0x7A); }

    // ---------- macOS 26 Liquid Glass 统一自绘原语（Widget / ThumbnailPopupWidget / 对话框复用） ----------
    /// 绘制完整的 Liquid Glass 连续曲率玻璃主体（底色 + 光学纵深渐变 + 顶部内高光 + 全周折光斜边）
    void paintLiquidGlassSurface(QPainter& painter, const QRectF& rect, qreal radius, bool dark, bool transparent);
    /// 绘制同心内凹舞台光学井（含平滑悬停微光插值 hoverProgress ∈ [0, 1]）
    void paintLiquidGlassStageWell(QPainter& painter, const QRectF& stageBox, qreal radius, qreal hoverProgress, bool dark);
    /// 仅绘制 Liquid Glass 对话框/气泡顶部的精细内受光缘与折光微边（叠加在 QSS 圆角帧之上）
    void paintLiquidGlassWindowRim(QPainter& painter, const QRectF& rect, qreal radius, bool dark);

    // ---------- 托盘菜单与窗口样式表声明（实现在 Theme.cpp 中，避免大体积字符串多处展开并加速编译） ----------
    QString menuStyleSheet();
    QString macDialogStyleSheet();
} // namespace Theme

#endif //WIN_SWITCHER_THEME_H
