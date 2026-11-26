#ifndef WIN_SWITCHER_THEME_H
#define WIN_SWITCHER_THEME_H

#include <QColor>
#include <QGuiApplication>
#include <QRectF>
#include <QStyleHints>
#include <QString>
#include "ConfigManager.h"

class QPainter;

/// macOS 26 (Tahoe / Liquid Glass) 深/浅色自适应主题引擎：
/// 默认跟随系统（Windows 应用模式）与「透明效果」辅助功能设置自动切换；
/// 可在 config.ini 中写 `ThemeMode = light / dark / auto`（或 0/1/2）强制指定。
/// 所有自定义绘制的配色与 Liquid Glass 光学材质均统一从此模块获取，保证全应用视觉语言一致。
namespace Theme {
    // ---------- 基础色常量（同一色值只定义一处；alpha 的各自微差在调用点表达） ----------
    constexpr int BrandR = 162, BrandG = 210, BrandB = 255; // 品牌色（马卡龙·婴儿蓝 #A2D2FF）
    constexpr int LightSurfaceR = 246, LightSurfaceG = 248, LightSurfaceB = 252; // 浅色 Liquid Glass 基值
    inline QColor brandColor(int alpha) { return QColor(BrandR, BrandG, BrandB, alpha); }
    inline QColor lightSurface(int alpha) { return QColor(LightSurfaceR, LightSurfaceG, LightSurfaceB, alpha); }

    /// 当前生效的深色与否（配置优先，其次系统；CFG 内部已做缓存，无逐帧 QSettings 开销）
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

    /// 系统「透明效果」可用性（缓存并在 refreshSystemVisualSettings 时刷新；关闭时自动回退高对比实体表面）
    bool isTransparencyEnabled();
    void refreshSystemVisualSettings();

    // ---------- 切换器弹窗（Widget / ThumbnailPopupWidget）macOS 26 Liquid Glass 配色 ----------
    inline QColor panelFill(bool dark, bool transparent) {
        if (dark)
            return transparent ? QColor(24, 26, 32, 196) : QColor(30, 32, 38, 248);
        return transparent ? QColor(246, 248, 252, 206) : QColor(244, 246, 250, 250);
    }
    inline QColor panelFill() { return panelFill(isDark(), isTransparencyEnabled()); }
    inline QColor panelStroke() { return isDark() ? QColor(255, 255, 255, 48) : QColor(0, 0, 0, 28); }
    inline QColor labelColor() { return isDark() ? QColor(245, 245, 248) : QColor(24, 24, 28); }

    /// 悬停态：Liquid Glass 轻磨砂胶囊 + 细微边缘光
    inline QColor hoverFill() { return isDark() ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 14); }
    inline QColor hoverStroke() { return isDark() ? QColor(255, 255, 255, 34) : QColor(255, 255, 255, 130); }

    /// 选中态：macOS 26 Liquid Glass 聚光透镜（延续经典马卡龙·婴儿蓝基因，赋予玻璃渐变与高光折射）
    inline QColor selectionFill() { return isDark() ? QColor(162, 210, 255, 224) : QColor(126, 192, 255, 212); }
    inline QColor selectionFillTop() { return isDark() ? QColor(182, 222, 255, 228) : QColor(152, 206, 255, 218); }
    inline QColor selectionFillBottom() { return isDark() ? QColor(134, 192, 255, 192) : QColor(102, 174, 255, 186); }
    inline QColor selectionStroke() { return isDark() ? QColor(255, 255, 255, 105) : QColor(0, 122, 255, 82); }
    inline QColor selectionInnerHighlight() { return isDark() ? QColor(255, 255, 255, 120) : QColor(255, 255, 255, 175); }

    // ---------- 舞台内凹光学井配色 ----------
    inline QColor stageEmptyText(bool dark) { return dark ? QColor(255, 255, 255, 145) : QColor(28, 28, 32, 140); }
    inline QColor stageEmptyText() { return stageEmptyText(isDark()); }

    // ---------- 面板背景材质（保留兼容访问器） ----------
    inline QColor panelGradTop() { return isDark() ? QColor(255, 255, 255, 22) : QColor(255, 255, 255, 95); }
    inline QColor panelGradBottom() { return isDark() ? QColor(0, 0, 0, 30) : QColor(180, 192, 212, 30); }
    inline QColor panelTopSheen() { return isDark() ? QColor(255, 255, 255, 78) : QColor(255, 255, 255, 215); }

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
