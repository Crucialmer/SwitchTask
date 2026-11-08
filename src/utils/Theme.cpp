#include "utils/Theme.h"
#include <QStringLiteral>

namespace Theme {

    QString menuStyleSheet() {
        const bool dark = isDark();
        auto css = [](const QColor& c) { return c.name(QColor::HexArgb); };
        // macOS 风格：通透毛玻璃底色（约 78%~80% alpha，配合 SetWindowCompositionAttribute 呈现通透的环境磨砂光影）
        const QColor bg = dark ? QColor(28, 28, 32, 198) : QColor(248, 248, 250, 204);
        const QColor border = dark ? QColor(255, 255, 255, 38) : QColor(0, 0, 0, 26);
        const QColor text = dark ? QColor(242, 242, 245) : QColor(28, 28, 30);
        const QColor disabled = dark ? QColor(255, 255, 255, 120) : QColor(0, 0, 0, 110);
        const QColor separator = dark ? QColor(255, 255, 255, 26) : QColor(0, 0, 0, 18);
        const QColor badgeBg = dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 14);

        // 顶栏专属卡片底色与边框（macOS Menu Bar Extra 顶栏状态卡片，对标 Tcursor 2.5.2）
        const QColor cardBg = dark ? QColor(46, 46, 51, 235) : QColor(242, 242, 246, 235);
        const QColor cardBorder = dark ? QColor(68, 68, 74, 200) : QColor(220, 220, 226, 220);
        const QColor cardHover = dark ? QColor(56, 56, 62, 245) : QColor(234, 234, 240, 245);
        const QColor statusText = dark ? QColor(242, 242, 245) : QColor(28, 28, 30);

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

        // 字号：默认 12.0pt，支持在 config.ini [menu] 节微调
        auto fontSize = CFG.get("menu/font_size", 12.0).toDouble();
        if (fontSize <= 0) fontSize = 12.0;

        QString qss = QStringLiteral(
                   "QMenu {"
                   "  background-color: %1;"
                   "  border: 1px solid %2;"
                   "  border-radius: 8px;"
                   "  padding: 4px;"
                   "  min-width: 168px;"
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
                   "  border-radius: 7px;"
                   "}"
                   "#menuHeaderCard:hover {"
                   "  background-color: @CARD_HOVER@;"
                   "  border: 1px solid rgba(0, 122, 255, 70);"
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
                   "  border-radius: 4px;"
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
                   "  padding: 4px 14px 4px 24px;"
                   "  border-radius: 4px;"
                   "  margin: 1px 2px;"
                   "  color: %5;"
                   "  background: transparent;"
                   "  font-weight: %4;"
                   "}"
                   "QMenu::item:selected { background-color: %8; color: %9; }"
                   "QMenu::item:disabled { color: %6; }"
                   "QMenu::separator { height: 1px; background: @SEP@; margin: 3px 10px; }"
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

        const QString frameBg = dark ? QStringLiteral("rgba(30, 30, 35, 235)") : QStringLiteral("rgba(246, 246, 248, 238)");
        const QString frameBorder = dark ? QStringLiteral("rgba(255, 255, 255, 38)") : QStringLiteral("rgba(0, 0, 0, 30)");
        const QString text = dark ? QStringLiteral("#F2F2F5") : QStringLiteral("#1C1C1E");
        const QString dim = dark ? QStringLiteral("#A1A1A6") : QStringLiteral("#6E6E73");
        const QString cardBg = dark ? QStringLiteral("rgba(255, 255, 255, 12)") : QStringLiteral("rgba(255, 255, 255, 180)");
        const QString cardBorder = dark ? QStringLiteral("rgba(255, 255, 255, 20)") : QStringLiteral("rgba(0, 0, 0, 16)");
        const QString btnSecBg = dark ? QStringLiteral("rgba(255, 255, 255, 22)") : QStringLiteral("rgba(0, 0, 0, 14)");
        const QString btnSecHover = dark ? QStringLiteral("rgba(255, 255, 255, 36)") : QStringLiteral("rgba(0, 0, 0, 24)");
        const QString btnSecBorder = dark ? QStringLiteral("rgba(255, 255, 255, 30)") : QStringLiteral("rgba(0, 0, 0, 24)");
        const QString progBg = dark ? QStringLiteral("rgba(255, 255, 255, 25)") : QStringLiteral("rgba(0, 0, 0, 20)");

        return QStringLiteral(
            "#macWindowFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 12px;"
            "  font-family: %3;"
            "}"
            "#macToastFrame {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 12px;"
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
            "  border-radius: 9px;"
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
            "  border-radius: 4px;"
            "  padding: 1px 7px;"
            "}"
            "QLabel#badge_green {"
            "  background-color: #34C759;"
            "  color: #FFFFFF;"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  border-radius: 4px;"
            "  padding: 1px 7px;"
            "}"
            "QLabel#badge_gray {"
            "  background-color: %8;"
            "  color: %7;"
            "  font-family: %3;"
            "  font-size: 11px;"
            "  font-weight: 500;"
            "  border-radius: 4px;"
            "  padding: 1px 7px;"
            "  border: 1px solid %9;"
            "}"
            "QFrame#card_notes {"
            "  background-color: %5;"
            "  border: 1px solid %6;"
            "  border-radius: 8px;"
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
            "  border-radius: 4px;"
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
            "  border-radius: 6px;"
            "  padding: 6px 18px;"
            "  border: none;"
            "}"
            "QPushButton#btn_primary:hover, QPushButton#btn_update:hover, QPushButton#btn_done:hover {"
            "  background-color: #0066D6;"
            "}"
            "QPushButton#btn_primary:pressed, QPushButton#btn_update:pressed, QPushButton#btn_done:pressed {"
            "  background-color: #0051B3;"
            "}"
            "QPushButton#btn_primary:disabled, QPushButton#btn_update:disabled, QPushButton#btn_done:disabled {"
            "  background-color: rgba(0, 122, 255, 90);"
            "  color: rgba(255, 255, 255, 140);"
            "}"
            "QPushButton#btn_secondary, QPushButton#btn_recheck, QPushButton#btn_config, QPushButton#btn_online {"
            "  background-color: %8;"
            "  color: %4;"
            "  font-family: %3;"
            "  font-size: 12px;"
            "  font-weight: 500;"
            "  border-radius: 6px;"
            "  padding: 6px 14px;"
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
