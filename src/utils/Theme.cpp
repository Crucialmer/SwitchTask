#include "utils/Theme.h"
#include "utils/Util.h"
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QStringLiteral>

namespace Theme {
    namespace {
        bool g_transparencyInitialized = false;
        bool g_transparencyEnabled = true;

        QColor lerpColor(const QColor& a, const QColor& b, qreal t) {
            const qreal clamped = qBound(0.0, t, 1.0);
            return QColor(
                qRound(a.red() + (b.red() - a.red()) * clamped),
                qRound(a.green() + (b.green() - a.green()) * clamped),
                qRound(a.blue() + (b.blue() - a.blue()) * clamped),
                qRound(a.alpha() + (b.alpha() - a.alpha()) * clamped)
            );
        }

        QString resolveUiFontFamily() {
            const auto customFontFamily = CFG.get("menu/font_family", "").toString().trimmed();
            if (!customFontFamily.isEmpty()) {
                if (customFontFamily.contains('\'') || customFontFamily.contains('"') || customFontFamily.contains(',')) {
                    return customFontFamily + QStringLiteral(", 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif");
                }
                return QStringLiteral("'%1', 'Microsoft YaHei UI', 'Microsoft YaHei', sans-serif").arg(customFontFamily);
            }
            return QStringLiteral("'Microsoft YaHei UI', 'Microsoft YaHei', 'Segoe UI Variable Text', 'Segoe UI', -apple-system, sans-serif");
        }
    } // anonymous namespace

    bool isTransparencyEnabled() {
        if (!g_transparencyInitialized) {
            g_transparencyEnabled = Util::isSystemTransparencyEnabled();
            g_transparencyInitialized = true;
        }
        return g_transparencyEnabled;
    }

    void refreshSystemVisualSettings() {
        g_transparencyEnabled = Util::isSystemTransparencyEnabled();
        g_transparencyInitialized = true;
    }

    void paintLiquidGlassSurface(QPainter& painter, const QRectF& rect, qreal radius, bool dark, bool transparent) {
        if (rect.width() <= 2.0 || rect.height() <= 2.0) return;

        const QRectF outerRect = rect.adjusted(0.5, 0.5, -0.5, -0.5);
        const QPainterPath panel = Util::continuousRoundedRectPath(outerRect, radius);

        // 1. Liquid Glass 通透玻璃基底（系统关闭透明效果时自动提升至实体高对比度底色）
        painter.setPen(Qt::NoPen);
        painter.setBrush(panelFill(dark, transparent));
        painter.drawPath(panel);

        // 2. 光学纵深漫反射层（顶部受光通透 -> 底部微妙沉降）
        {
            QLinearGradient shade(outerRect.topLeft(), outerRect.bottomLeft());
            if (dark) {
                shade.setColorAt(0.0, QColor(255, 255, 255, 22));
                shade.setColorAt(0.36, Qt::transparent);
                shade.setColorAt(0.76, Qt::transparent);
                shade.setColorAt(1.0, QColor(0, 0, 0, 32));
            } else {
                shade.setColorAt(0.0, QColor(255, 255, 255, 105));
                shade.setColorAt(0.40, QColor(255, 255, 255, 14));
                shade.setColorAt(0.78, Qt::transparent);
                shade.setColorAt(1.0, QColor(175, 188, 210, 32));
            }
            painter.setBrush(shade);
            painter.drawPath(panel);
        }

        // 3. 顶部内切受光弧（模拟玻璃上缘弯曲折光高光）
        {
            const QRectF innerRect = rect.adjusted(1.5, 1.5, -1.5, -1.5);
            const QPainterPath inner = Util::continuousRoundedRectPath(innerRect, qMax(2.0, radius - 1.0));
            QLinearGradient topCatch(0.0, innerRect.top(), 0.0, innerRect.top() + 14.0);
            if (dark) {
                topCatch.setColorAt(0.0, QColor(255, 255, 255, 82));
                topCatch.setColorAt(0.45, QColor(255, 255, 255, 16));
                topCatch.setColorAt(1.0, Qt::transparent);
            } else {
                topCatch.setColorAt(0.0, QColor(255, 255, 255, 220));
                topCatch.setColorAt(0.45, QColor(255, 255, 255, 55));
                topCatch.setColorAt(1.0, Qt::transparent);
            }
            painter.setPen(QPen(QBrush(topCatch), 1.25));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(inner);
        }

        // 4. 全周折光斜边（macOS 26 Liquid Glass 标志性双轴对角镜面描边：左上高光 -> 侧翼柔和 -> 右下微弱反射）
        {
            QLinearGradient bevel(outerRect.topLeft(), outerRect.bottomRight());
            if (dark) {
                bevel.setColorAt(0.0, QColor(255, 255, 255, 96));
                bevel.setColorAt(0.32, QColor(255, 255, 255, 36));
                bevel.setColorAt(0.72, QColor(255, 255, 255, 22));
                bevel.setColorAt(1.0, QColor(255, 255, 255, 48));
            } else {
                bevel.setColorAt(0.0, QColor(255, 255, 255, 230));
                bevel.setColorAt(0.34, QColor(0, 0, 0, 22));
                bevel.setColorAt(0.72, QColor(0, 0, 0, 28));
                bevel.setColorAt(1.0, QColor(255, 255, 255, 130));
            }
            painter.setPen(QPen(QBrush(bevel), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(panel);
        }
    }

    void paintLiquidGlassStageWell(QPainter& painter, const QRectF& stageBox, qreal radius, qreal hoverProgress, bool dark) {
        if (stageBox.width() <= 4.0 || stageBox.height() <= 4.0) return;

        const qreal t = qBound(0.0, hoverProgress, 1.0);
        const QPainterPath stagePath = Util::continuousRoundedRectPath(stageBox, radius);

        // 1. 内凹光学井底衬（深色下呈现深邃石墨井，浅色下呈现通透微暗冰井）
        QLinearGradient well(0.0, stageBox.top(), 0.0, stageBox.bottom());
        if (dark) {
            well.setColorAt(0.0, QColor(0, 0, 0, 64));
            well.setColorAt(1.0, QColor(0, 0, 0, 38));
        } else {
            well.setColorAt(0.0, QColor(15, 23, 42, 20));
            well.setColorAt(1.0, QColor(15, 23, 42, 10));
        }

        // 悬停时：边框柔和过渡至 Liquid Glass 聚焦微光（带微妙品牌蓝/苹果蓝光晕）
        const QColor restBorder = dark ? QColor(255, 255, 255, 30) : QColor(0, 0, 0, 24);
        const QColor hoverBorder = dark ? QColor(174, 218, 255, 138) : QColor(0, 122, 255, 122);
        const QColor borderColor = lerpColor(restBorder, hoverBorder, t);

        painter.setBrush(well);
        painter.setPen(QPen(borderColor, t > 0.01 ? 1.25 : 1.0));
        painter.drawPath(stagePath);

        // 2. 舞台内缘顶部 1px 漫反射微光线（随悬停柔亮）
        const QPainterPath innerSheen = Util::continuousRoundedRectPath(stageBox.adjusted(1.0, 1.0, -1.0, -1.0), qMax(2.0, radius - 1.0));
        QLinearGradient sheenGrad(0.0, stageBox.top(), 0.0, stageBox.top() + 6.0);
        const QColor restSheen = dark ? QColor(255, 255, 255, 38) : QColor(255, 255, 255, 140);
        const QColor hoverSheen = dark ? QColor(210, 236, 255, 110) : QColor(255, 255, 255, 225);
        sheenGrad.setColorAt(0.0, lerpColor(restSheen, hoverSheen, t));
        sheenGrad.setColorAt(1.0, Qt::transparent);
        painter.setPen(QPen(QBrush(sheenGrad), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(innerSheen);
    }

    void paintLiquidGlassWindowRim(QPainter& painter, const QRectF& rect, qreal radius, bool dark) {
        if (rect.width() <= 4.0 || rect.height() <= 4.0) return;

        const QRectF innerRect = rect.adjusted(1.5, 1.5, -1.5, -1.5);
        const QPainterPath inner = Util::continuousRoundedRectPath(innerRect, qMax(2.0, radius - 1.0));
        QLinearGradient rim(0.0, innerRect.top(), 0.0, innerRect.top() + 16.0);
        if (dark) {
            rim.setColorAt(0.0, QColor(255, 255, 255, 62));
            rim.setColorAt(0.5, QColor(255, 255, 255, 12));
            rim.setColorAt(1.0, Qt::transparent);
        } else {
            rim.setColorAt(0.0, QColor(255, 255, 255, 210));
            rim.setColorAt(0.5, QColor(255, 255, 255, 45));
            rim.setColorAt(1.0, Qt::transparent);
        }
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QBrush(rim), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(inner);
        painter.restore();
    }

    QString menuStyleSheet() {
        const bool dark = isMenuDark();
        const bool transparent = isTransparencyEnabled();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };

        // macOS 26 Tahoe Liquid Glass 托盘浮岛菜单
        const QColor bg = dark
            ? (transparent ? QColor(26, 28, 34, 202) : QColor(32, 34, 40, 250))
            : (transparent ? QColor(248, 249, 252, 210) : QColor(246, 247, 250, 250));
        const QColor border = dark ? QColor(255, 255, 255, 46) : QColor(0, 0, 0, 28);
        const QColor text = dark ? QColor(244, 244, 247) : QColor(24, 24, 28);
        const QColor disabled = dark ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 110);
        const QColor separator = dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 18);
        const QColor badgeBg = dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 14);

        // 顶栏专属玻璃卡片底色与边框
        const QColor cardBg = dark ? QColor(255, 255, 255, 18) : QColor(255, 255, 255, 195);
        const QColor cardBorder = dark ? QColor(255, 255, 255, 32) : QColor(0, 0, 0, 18);
        const QColor cardHover = dark ? QColor(255, 255, 255, 30) : QColor(255, 255, 255, 240);
        const QColor statusText = dark ? QColor(235, 235, 240) : QColor(42, 42, 46);

        // 选中悬停态（macOS 26 胶囊高亮）
        const QColor selBg = dark ? QColor(255, 255, 255, 36) : QColor(0, 122, 255);
        const QColor selText = QColor(255, 255, 255);
        const QString checkIcon = dark ? QStringLiteral(":/img/checkmark-dark.svg") : QStringLiteral(":/img/checkmark-light.svg");
        const QString fontFamily = resolveUiFontFamily();

        const auto weightRaw = CFG.get("menu/font_weight", "bold").toString().trimmed().toLower();
        int weight = 700;
        if (weightRaw == "bold" || weightRaw == "b" || weightRaw == "700") weight = 700;
        else if (weightRaw == "semibold" || weightRaw == "demibold" || weightRaw == "600") weight = 600;
        else if (weightRaw == "medium" || weightRaw == "500") weight = 500;
        else if (weightRaw == "normal" || weightRaw == "regular" || weightRaw == "400") weight = 400;
        else if (auto val = weightRaw.toInt(); val >= 100 && val <= 900) weight = val;

        auto fontSize = CFG.get("menu/font_size", 12.0).toDouble();
        if (fontSize <= 0) fontSize = 12.0;

        QString qss = QStringLiteral(
                   "QMenu {"
                   "  background-color: %1;"
                   "  border: 1px solid %2;"
                   "  border-radius: 12px;"
                   "  padding: 5px;"
                   "  min-width: 174px;"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: %3px;"
                   "  font-weight: %4;"
                   "}"
                   "#menuHeaderWidget {"
                   "  background: transparent;"
                   "  padding: 1px 1px 3px 1px;"
                   "}"
                   "#menuHeaderCard {"
                   "  background-color: @CARD_BG@;"
                   "  border: 1px solid @CARD_BORDER@;"
                   "  border-radius: 9px;"
                   "}"
                   "#menuHeaderCard:hover {"
                   "  background-color: @CARD_HOVER@;"
                   "  border: 1px solid rgba(0, 122, 255, 95);"
                   "}"
                   "#menuHeaderTitle {"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: 12.5px;"
                   "  font-weight: 700;"
                   "  color: %5;"
                   "  background: transparent;"
                   "}"
                   "#menuHeaderVersion {"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: 10.5px;"
                   "  font-weight: 500;"
                   "  color: %6;"
                   "  background-color: %7;"
                   "  border-radius: 5px;"
                   "  padding: 1px 6px;"
                   "}"
                   "#menuHeaderStatusText {"
                   "  font-family: @FONT_FAMILY@;"
                   "  font-size: 10.5px;"
                   "  font-weight: %4;"
                   "  color: @STATUS_TEXT@;"
                   "  background: transparent;"
                   "}"
                   "QMenu::item {"
                   "  padding: 5px 14px 5px 25px;"
                   "  border-radius: 6px;"
                   "  margin: 1px 2px;"
                   "  color: %5;"
                   "  background: transparent;"
                   "  font-weight: %4;"
                   "}"
                   "QMenu::item:selected { background-color: %8; color: %9; }"
                   "QMenu::item:disabled { color: %6; }"
                   "QMenu::separator { height: 1px; background: @SEP@; margin: 4px 10px; }"
                   "QMenu::indicator { width: 11px; height: 11px; margin-left: 8px; }"
                   "QMenu::indicator:checked { image: url(@CHK@); }"
                   "QMenu::indicator:checked:selected { image: url(:/img/checkmark-dark.svg); }"
                   "QMenu::indicator:unchecked { image: none; border: none; background: transparent; }"
                   "QMenu::right-arrow { image: url(:/img/chevron-right.svg); width: 10px; height: 10px; margin-right: 6px; }")
            .arg(css(bg), css(border), QString::number(fontSize), QString::number(weight), css(text), css(disabled), css(badgeBg), css(selBg), css(selText));
        qss.replace("@SEP@", css(separator));
        qss.replace("@CHK@", checkIcon);
        qss.replace("@FONT_FAMILY@", fontFamily);
        qss.replace("@CARD_BG@", css(cardBg));
        qss.replace("@CARD_BORDER@", css(cardBorder));
        qss.replace("@CARD_HOVER@", css(cardHover));
        qss.replace("@STATUS_TEXT@", css(statusText));
        return qss;
    }

    QString macDialogStyleSheet() {
        const bool dark = isDark();
        const bool transparent = isTransparencyEnabled();
        const QString fontFamily = resolveUiFontFamily();

        const QString frameBg = dark
            ? (transparent ? QStringLiteral("rgba(26, 28, 34, 228)") : QStringLiteral("rgba(30, 32, 38, 250)"))
            : (transparent ? QStringLiteral("rgba(246, 248, 252, 232)") : QStringLiteral("rgba(245, 247, 250, 250)"));
        const QString frameBorder = dark ? QStringLiteral("rgba(255, 255, 255, 46)") : QStringLiteral("rgba(0, 0, 0, 28)");
        const QString text = dark ? QStringLiteral("#F5F5F7") : QStringLiteral("#1C1C1E");
        const QString dim = dark ? QStringLiteral("#A1A1A6") : QStringLiteral("#6E6E73");
        const QString cardBg = dark ? QStringLiteral("rgba(255, 255, 255, 14)") : QStringLiteral("rgba(255, 255, 255, 192)");
        const QString cardBorder = dark ? QStringLiteral("rgba(255, 255, 255, 24)") : QStringLiteral("rgba(0, 0, 0, 15)");
        const QString btnSecBg = dark ? QStringLiteral("rgba(255, 255, 255, 20)") : QStringLiteral("rgba(0, 0, 0, 11)");
        const QString btnSecHover = dark ? QStringLiteral("rgba(255, 255, 255, 34)") : QStringLiteral("rgba(0, 0, 0, 20)");
        const QString btnSecBorder = dark ? QStringLiteral("rgba(255, 255, 255, 34)") : QStringLiteral("rgba(0, 0, 0, 20)");
        const QString progBg = dark ? QStringLiteral("rgba(255, 255, 255, 24)") : QStringLiteral("rgba(0, 0, 0, 18)");

        return QStringLiteral(
            "#macWindowFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 16px;"
            "  font-family: %3;"
            "}"
            "#macToastFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 16px;"
            "}"
            "#macToastTitle {"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  font-weight: 700;"
            "  color: %4;"
            "  background: transparent;"
            "}"
            "#macToastTime {"
            "  font-family: %3;"
            "  font-size: 10px;"
            "  font-weight: 500;"
            "  color: %7;"
            "  background: transparent;"
            "}"
            "#macToastMessage {"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "  color: %7;"
            "  background: transparent;"
            "}"
            "#macTitleBarTitle {"
            "  font-family: %3;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "  color: %4;"
            "  background: transparent;"
            "}"
            "#macCard {"
            "  background-color: %5;"
            "  border: 1px solid %6;"
            "  border-radius: 12px;"
            "}"
            "QScrollArea#settingsScrollArea, QWidget#settingsScrollContent, QWidget#settingsBottomBar {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QLabel {"
            "  font-family: %3;"
            "  color: %4;"
            "  font-size: 12px;"
            "  background: transparent;"
            "}"
            "QLabel#label_dim {"
            "  color: %7;"
            "  font-size: 11px;"
            "}"
            "QLabel#label_section {"
            "  font-weight: 650;"
            "  font-size: 12.5px;"
            "  color: %4;"
            "}"
            "QLabel#label_heroTitle, QLabel#label_appName {"
            "  font-family: %3;"
            "  font-size: 15.5px;"
            "  font-weight: 700;"
            "  color: %4;"
            "  background: transparent;"
            "}"
            "QLabel#badge_blue {"
            "  background-color: #007AFF;"
            "  color: #FFFFFF;"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  border-radius: 9px;"
            "  padding: 2px 9px;"
            "}"
            "QLabel#badge_green {"
            "  background-color: #34C759;"
            "  color: #FFFFFF;"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  border-radius: 9px;"
            "  padding: 2px 9px;"
            "}"
            "QLabel#badge_gray {"
            "  background-color: %8;"
            "  color: %7;"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "  border-radius: 9px;"
            "  padding: 2px 9px;"
            "  border: 1px solid %9;"
            "}"
            "QFrame#card_notes {"
            "  background-color: %5;"
            "  border: 1px solid %6;"
            "  border-radius: 12px;"
            "  padding: 6px 10px;"
            "}"
            "QTextBrowser {"
            "  background-color: transparent;"
            "  border: none;"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  color: %4;"
            "  selection-background-color: #007AFF;"
            "}"
            "QScrollBar:vertical {"
            "  width: 6px;"
            "  background: transparent;"
            "  margin: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: %8;"
            "  border-radius: 3px;"
            "  min-height: 20px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background: %10;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "  height: 0px;"
            "}"
            "QCheckBox, QRadioButton {"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "  color: %4;"
            "  spacing: 8px;"
            "  background: transparent;"
            "}"
            "QCheckBox::indicator, QRadioButton::indicator {"
            "  width: 15px;"
            "  height: 15px;"
            "  border-radius: 5px;"
            "  border: 1px solid %6;"
            "  background-color: %5;"
            "}"
            "QRadioButton::indicator { border-radius: 7px; }"
            "QCheckBox::indicator:checked, QRadioButton::indicator:checked {"
            "  background-color: #007AFF;"
            "  border-color: #007AFF;"
            "}"
            "QPushButton#btn_primary, QPushButton#btn_update, QPushButton#btn_done {"
            "  background-color: #007AFF;"
            "  color: #FFFFFF;"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  font-weight: 600;"
            "  border-radius: 14px;"
            "  padding: 6px 20px;"
            "  border: 1px solid rgba(255, 255, 255, 45);"
            "}"
            "QPushButton#btn_primary:hover, QPushButton#btn_update:hover, QPushButton#btn_done:hover {"
            "  background-color: #0A84FF;"
            "}"
            "QPushButton#btn_primary:pressed, QPushButton#btn_update:pressed, QPushButton#btn_done:pressed {"
            "  background-color: #005ECB;"
            "}"
            "QPushButton#btn_primary:disabled, QPushButton#btn_update:disabled, QPushButton#btn_done:disabled {"
            "  background-color: rgba(0, 122, 255, 90);"
            "  color: rgba(255, 255, 255, 140);"
            "  border-color: transparent;"
            "}"
            "QPushButton#btn_secondary, QPushButton#btn_recheck, QPushButton#btn_config, QPushButton#btn_online {"
            "  background-color: %8;"
            "  color: %4;"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "  border-radius: 14px;"
            "  padding: 6px 16px;"
            "  border: 1px solid %9;"
            "}"
            "QPushButton#btn_secondary:hover, QPushButton#btn_recheck:hover, QPushButton#btn_config:hover, QPushButton#btn_online:hover {"
            "  background-color: %10;"
            "}"
            "QProgressBar {"
            "  background-color: %11;"
            "  border-radius: 4px;"
            "  height: 8px;"
            "  text-align: center;"
            "  border: none;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #007AFF;"
            "  border-radius: 4px;"
            "}"
        ).arg(frameBg, frameBorder, fontFamily, text, cardBg, cardBorder, dim, btnSecBg, btnSecBorder, btnSecHover, progBg);
    }

} // namespace Theme
