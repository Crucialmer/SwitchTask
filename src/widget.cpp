#include "../header/widget.h"
#include "ui_Widget.h"
#include "utils/Util.h"
#include <QDebug>
#include <QWindow>
#include <QScreen>
#include "utils/setWindowBlur.h"
#include "utils/IconOnlyDelegate.h"
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QDateTime>
#include "utils/QtWin.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTimer>
#include <QRandomGenerator>
#include <QImage>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QMetaEnum>
#include <QFontMetrics>
#include <QSet>
#include <QHash>
#include "utils/SystemTray.h"
#include "utils/ConfigManager.h"
#include "utils/Theme.h"

// 图标模式：true=优先每个窗口自身的图标（可区分不同文件夹/头像；源图通常 32px，放大到 64px 实测偏糊）；
// false=统一应用图标（锐利）+ QQ 窗口叠加聊天头像角标。2026-10-11 实测后采用 false（B 方案）
static constexpr bool USE_WINDOW_ICON = false;

// macOS 风格：图标单元格与底部标题的固定间距（垂直留白规则：图标顶边距 == 文字底边距，标题字号变化时底部留白随之调整）
// 视觉上"图标底 → 文字顶"距离 = 图标内缩(8) + 本值；16 为增加呼吸感后的取值（原 8：选中项放大后观感偏紧）
static constexpr int kCellLabelGap = 16;

Widget::Widget(QWidget* parent) : QWidget(parent), ui(new Ui::Widget) {
    ui->setupUi(this);
    lw = ui->listWidget;
    ui->label->setTextFormat(Qt::PlainText); // 标题按纯文本渲染：防止窗口标题中的“伪 HTML”片段被富文本解析（省略号宽度按纯文本计算，两者保持一致）
    setWindowFlag(Qt::WindowStaysOnTopHint);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground); //设置窗口背景透明 !但是会造成show()时的闪烁 和 绘制延迟(?)
    QtWin::taskbarDeleteTab(this); //删除任务栏图标
    setWindowTitle("SwitchTask");

    Util::setWindowRoundCorner(this->hWnd()); // 设置窗口圆角
    // 窗口材质（磨砂模糊）统一在 applyTheme() 内应用（须配合 WA_TranslucentBackground；见下）

    setupLabelFont();

    lw->setViewMode(QListView::IconMode);
    lw->setMovement(QListView::Static);
    lw->setFlow(QListView::LeftToRight);
    lw->setWrapping(false);
    lw->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lw->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lw->setIconSize({64, 64});
    lw->setGridSize({80, 80});
    lw->setFixedHeight(lw->gridSize().height());
    lw->setUniformItemSizes(true); // optimization ?
    lw->setStyleSheet(R"(
        QListWidget {
            background-color: transparent;
            border: none;
            outline: none; /* 去除选中时的虚线框（在文字为空时，会形成闪电一样的标志 离谱） */
        }
    )");
    // 就算Text为Null，也会占用空间，很难做到真正的IConMode，所以只能delegate自绘
    // 本来为了去除图标选中变色样式，可以对Icon手动addPixmap(..., QIcon::Selected) or (& ~Qt::ItemIsSelectable)
    // 但是采用delegate后，就没必要了
    // will not take ownership of delegate
    iconDelegate = new IconOnlyDelegate(lw, Theme::selectionFill(), Theme::hoverFill());
    lw->setItemDelegate(iconDelegate);

    // macOS 风格：选中项放大动效（0→1，约 110ms，OutCubic）——由选中变化与弹窗显示触发；
    // 动画步进写入 delegate 并触发 viewport 重绘（含放大后与相邻单元格的重叠区域）
    selectionGrowAnim = new QVariantAnimation(this);
    selectionGrowAnim->setStartValue(0.0);
    selectionGrowAnim->setEndValue(1.0);
    selectionGrowAnim->setDuration(110);
    selectionGrowAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(selectionGrowAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        if (iconDelegate)
            iconDelegate->setSelectionGrow(v.toReal());
        lw->viewport()->update();
    });
    lw->installEventFilter(this);
    // 鼠标事件实际投递给列表的 viewport（不是 QListWidget 本身，QAbstractScrollArea 子类的经典坑）——
    // 必须同时装在 viewport 上，否则点击图标的按下事件会被列表默认处理消费，过滤器收不到（单击切换失效的根因）
    lw->viewport()->installEventFilter(this);
    // 悬停实时选中：开启 viewport 的鼠标跟踪——无按键按下的移动事件才会投递（eventFilter 的 MouseMove 分支依赖）
    lw->viewport()->setMouseTracking(true);

    // 深浅色主题：面板/标题/选中底配色随系统自动切换（config.ini 的 ThemeMode 可强制 light/dark）
    applyTheme();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
        applyTheme(); // 系统深浅色切换时即时刷新（弹窗保持可见时也立即生效）
    });
    connect(&CFG, &ConfigManager::configEdited, this, [this] {
        // 强制模式同步（applyConfiguredScheme）由 SystemTray 的 configEdited 处理器统一负责
        // （其连接先于本处注册、同一事件内先行执行）；此处只应用配色——applyTheme 读取的
        // isDark() 直接取配置，不依赖 Qt 侧调色板，因此与调用顺序无关
        applyTheme();
    });

    connect(lw, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* cur, QListWidgetItem*) {
        if (cur) {
            showLabelForItem(cur);
            startSelectionGrow(); // macOS 风格：选中项放大动效（快速切换时呈“跟随放大”效果）
        }
    });

    // 单击切换的兜底路径：若某环境下过滤器仍未截获按下事件（由列表默认处理消费），
    // itemPressed 信号依然会带条目命中信息（pressed 时鼠标左键须处于按下态，以排除右键等其它按键）。
    // 正常路径（过滤器已消费事件）不会触发本信号，因此不会双发
    connect(lw, &QListWidget::itemPressed, this, [this](QListWidgetItem* item) {
        if (QApplication::mouseButtons() & Qt::LeftButton)
            handleIconClick(item);
        else if (QApplication::mouseButtons() & Qt::MiddleButton)
            closeWindowItem(item); // 中键关闭的兜底路径（与过滤器互斥，不会双发）
    });

    connect(qApp, &QApplication::focusWindowChanged, this, [this](QWindow* focusWindow) {
        if (focusWindow == nullptr) {
            if (!this->underMouse()) { // hide when lost focus & mouse outside (means user choose to)
                pinned = false; // 非 Enter 确认路径的隐藏都视为取消
                hide();
            } else { // Windows Terminal will do
                qWarning() << "Someone tried to steal focus!";
            }
        }
    });
}

Widget::~Widget() {
    delete ui;
}

void Widget::keyPressEvent(QKeyEvent* event) {
    auto key = event->key();
    auto modifiers = event->modifiers();
    static const QHash<int, int> VimArrows = {
        {Qt::Key_K, Qt::Key_Up},    // ↑
        {Qt::Key_J, Qt::Key_Down},  // ↓
        {Qt::Key_H, Qt::Key_Left},  // ←
        {Qt::Key_L, Qt::Key_Right}, // →
    };
    // Vim 键先行归一化为方向键：与 KeyboardHooker 的映射语义一致（钩子未生效时的兜底路径也走同一套处理）
    if (const int mapped = VimArrows.value(key, 0); mapped != 0)
        key = mapped;
    if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
        // 确认切换：pinned 模式的 Enter/空格；普通模式（按住Alt）也可空格提前确认
        if (this->isVisible()) {
            switchToCurrentItem();
            return;
        }
    } else if (key == Qt::Key_Escape) {
        // 弹出器显示中 Esc 取消（不切换，直接隐藏）——兼容非 pinned 呼出路径
        if (this->isVisible()) {
            pinned = false;
            hide();
            return;
        }
    } else if (key == Qt::Key_W && modifiers == Qt::ControlModifier) {
        // Ctrl+W：关闭当前选中的窗口（与鼠标中键等效；弹窗保持显示，可连续关闭）
        if (this->isVisible()) {
            closeCurrentItem();
            return;
        }
    } else if (key == Qt::Key_Tab || key == Qt::Key_Backtab) { // switch to next or prev
        cycleSelection(!((modifiers & Qt::ShiftModifier) || key == Qt::Key_Backtab)); // Backtab = Shift+Tab
    } else if (key == Qt::Key_QuoteLeft && this->isVisible()) {
        // 弹出器显示中：`（Tab 上方键）= 向左循环选择（与 Tab 向右对称）；Shift+` 向右
        // 注意：此分支在 Alt+` 同应用轮换分支之前——弹出器可见时 ` 键优先用于列表选择，
        // Alt+` 的同应用窗口轮换仅在弹出器不可见时生效（原语义保留）
        cycleSelection(modifiers & Qt::ShiftModifier);
    } else if (key == Qt::Key_QuoteLeft && (modifiers & Qt::AltModifier)) { // Alt + `, 在前台窗口同组窗口间切换（弹出器不可见时；可见时的 ` 已由上一分支处理为列表选择）
        auto foreWin = GetForegroundWindow();
        // 重建条件含“不含当前前台窗口”：击穿陈旧缓存（如任务栏滚轮遗留的分组、未观察到的 Alt 释放），避免轮换静默无动作
        if (groupWindowOrder.isEmpty() || !groupWindowOrder.contains(foreWin)) {
            auto targetExe = Util::getWindowProcessPath(foreWin);
            groupWindowOrder = buildGroupWindowOrder(targetExe);
        }
        if (auto nextWin = rotateWindowInGroup(groupWindowOrder, foreWin, !(modifiers & Qt::ShiftModifier))) {
            if (IsWindow(nextWin)) { // 句柄可能已失效（组内窗口刚关闭）：避免对死句柄执行切换
                Util::switchToWindow(nextWin, true);
                qInfo() << "(Alt+`)Switch to" << Util::getWindowTitle(nextWin) << Util::getClassName(nextWin);
            }
        }
    } else if (key == Qt::Key_Up || key == Qt::Key_Down) {
        if (auto item = lw->currentItem()) {
            auto center = lw->visualItemRect(item).center();
            // 转发映射到WheelEvent；伪造事件的 angleDelta 与 eventFilter 中读取的轴保持一致（x 分量）
            auto wheelEvent = new QWheelEvent(center, lw->mapToGlobal(center), {},
                                              {key == Qt::Key_Up ? 120 : -120, 0},
                                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::postEvent(lw, wheelEvent);
        }
    } else if (key == Qt::Key_Left || key == Qt::Key_Right) {
        // 左右循环选择（完整处理而非仅边界）：钩子转发的合成事件直达本函数，
        // 不经过列表控件的原生光标移动，因此这里需要处理全部位置
        cycleSelection(key == Qt::Key_Right);
    }
    QWidget::keyPressEvent(event);
}

bool Widget::forceShow() {
    setWindowOpacity(0.005); // 减少闪烁发生(因Qt::WA_TranslucentBackground) in showMinimized()
    showMinimized();
    showNormal();
    setWindowOpacity(1);
    startSelectionGrow(); // macOS 风格：弹出时选中项放大登场（每次呼出/抢回前台播放一次）
    return isForeground();
}

/// macOS 风格：播放“选中项放大”动效（0→1）；快速切换时停止并从头播放，形成跟随放大效果。
/// 仅在可见时播放：隐藏期间的选中变化（如列表重建）不必空跑，弹窗显示时由 forceShow 统一起始
void Widget::startSelectionGrow() {
    if (!selectionGrowAnim || !isVisible()) return;
    selectionGrowAnim->stop();
    selectionGrowAnim->start();
}

/// show App description under the icon
void Widget::showLabelForItem(QListWidgetItem* item) {
    if (!item) return;

    // 底部标签：显示窗口自身标题；标题为空时兜底为进程描述
    auto info = item->data(Qt::UserRole).value<WindowInfo>();
    QString text = info.title;
    if (text.isEmpty())
        text = Util::getFileDescription(info.exePath);
    // macOS 风格：标题超出面板可用宽度时中部省略（…）——此前直接 setText 时，超宽文本会左对齐并被面板右缘硬切
    const int maxTextWidth = qMax(40, this->width() - 16); // 距面板左右缘各留 8px
    const QFontMetrics fm(ui->label->font());
    ui->label->setText(fm.elidedText(text, Qt::ElideMiddle, maxTextWidth));
    ui->label->adjustSize();

    auto itemRect = lw->visualItemRect(item);
    const auto center = lw->mapTo(this, itemRect.center());
    auto labelRect = ui->label->rect();
    // 垂直：紧贴图标单元格下方 kCellLabelGap 处（面板底部留白由 prepareListWidget 的“等距”规则确定）
    labelRect.moveTop(center.y() + itemRect.height() / 2 + kCellLabelGap);
    labelRect.moveLeft(center.x() - labelRect.width() / 2);

    auto bound = this->rect().marginsRemoved({8, 0, 8, 0});
    labelRect.moveRight(qMin(labelRect.right(), bound.right()));
    labelRect.moveLeft(qMax(labelRect.left(), bound.left())); // 兜底夹取（省略后必然满足，防任何越界）

    ui->label->move(labelRect.topLeft());
}

void Widget::setupLabelFont() {
    static auto reloadLabelFontCfg = [this] {
        const QStringList Fonts = {"Microsoft YaHei UI", "Microsoft YaHei", "Consolas"}; // fallback
        auto labelFont = ui->label->font();
        auto fontSize = CFG.get("label/font_size", 10).toDouble(); // 支持小数，如 10.5（100% 缩放下即 14px）
        if (fontSize <= 0)
            fontSize = 10; // 防呆：非法值回退默认
        labelFont.setPointSizeF(fontSize);
        auto defaultFF = QStringList{CFG.get("label/font_family", Fonts[0]).toString()};
        labelFont.setFamilies(defaultFF << Fonts.mid(1));
        // font_weight 可选：数字(100~900，400=常规 500=中等 600=半粗 700=粗体) 或 bold/normal；不写=默认粗体(bold)
        const auto weightCfg = CFG.get("label/font_weight", "bold").toString().trimmed().toLower();
        int weight = 700; // 默认加粗（bold）
        if (!weightCfg.isEmpty()) {
            weight = -1;
            if (weightCfg == "bold" || weightCfg == "b")
                weight = 700;
            else if (weightCfg == "normal" || weightCfg == "regular" || weightCfg == "n")
                weight = 400;
            else {
                bool ok = false;
                const int num = weightCfg.toInt(&ok);
                if (ok && num >= 100 && num <= 900)
                    weight = num;
            }
            if (weight <= 0) {
                weight = 700; // 非法值：回退默认粗体
                qWarning() << "Invalid label/font_weight (expect 100~900/bold/normal):" << weightCfg;
            }
        }
        labelFont.setWeight(QFont::Weight(weight));
        ui->label->setFont(labelFont);
        qDebug() << labelFont.families();
        qDebug() << "Label Actual Font:" << QFontInfo(labelFont).family();
    };
    reloadLabelFontCfg();

    // auto reload
    connect(&CFG, &ConfigManager::configEdited, this, [] {
        reloadLabelFontCfg();
    });
}

/// 应用当前深浅色主题：底部标题颜色、列表选中/悬停/描边色、面板与列表重绘
void Widget::applyTheme() {
    // 窗口材质：磨砂模糊（BLURBEHIND）——透明窗口 + 模糊在真机上呈毛玻璃观感。
    // 注：2026-10-14 实测，ACRYLICBLURBEHIND（亚克力）在部分合成环境（RDP/虚拟 GPU）会被系统
    // 渲染为纯色实心层（忽略染色 alpha），把半透明填充的通透感整个盖死——故不启用；通透感由
    // 面板填充的 alpha 承载（系统不支持模糊时自动降级为纯半透明，观感依然成立）。
    // SetWindowCompositionAttribute 幂等——每次主题变更重新应用
    setWindowBlur(hWnd());
    ui->label->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::labelColor().name()));
    if (iconDelegate)
        iconDelegate->setColors(Theme::selectionFill(), Theme::hoverFill());
    // 面板配色缓存到成员：paintEvent 高频重绘（悬停/选中放大动效每帧 update）不再逐帧重读 QSettings
    panelFillColor = Theme::panelFill();
    panelStrokeColor = Theme::panelStroke();
    if (lw)
        lw->viewport()->update();
    update(); // 面板底色/描边重绘
}

void Widget::keyReleaseEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Alt) {
        groupWindowOrder.clear(); // for Alt + `
        // 方案A：Alt+Tab 呼出后一律 pinned——松开 Alt 不触发切换，保持显示，
        // 由 Enter/空格/双击确认、Esc 取消、失焦自动取消
        QWidget::keyReleaseEvent(event);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

/// 材质颗粒纹理笔刷（Apple 式细颗粒：质感 + 防色带）。
/// 固定种子 → 纹理跨运行稳定；绘制侧按设备像素比缩小 → 1 颗粒 = 1 设备像素（高 DPI 下依旧细腻）
static const QBrush& grainTextureBrush()
{
    static QBrush brush = [] {
        QImage tex(96, 96, QImage::Format_ARGB32_Premultiplied);
        tex.fill(Qt::transparent);
        QRandomGenerator rng(20261014); // 固定种子
        for (int y = 0; y < tex.height(); ++y)
            for (int x = 0; x < tex.width(); ++x) {
                const int v = int(rng.bounded(5)) - 2; // -2..2 级亮度
                if (v == 0)
                    continue;
                const int a = qAbs(v) * 3; // 3~6/255：细而低调
                tex.setPixelColor(x, y, QColor(v > 0 ? 255 : 0, v > 0 ? 255 : 0, v > 0 ? 255 : 0, a));
            }
        return QBrush(tex);
    }();
    return brush;
}

void Widget::paintEvent(QPaintEvent*) { //不绘制会导致鼠标穿透背景
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    // macOS 风格：圆角矩形背景（r=12——更圆润的整面观感；用户裁决 2026-10-14 C 方案）+ 细描边
    // ——不做过激的窗形裁剪：圆角由绘制路径呈现，1px 描边勾勒轮廓
    QPainterPath panel;
    panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
    painter.setPen(Qt::NoPen);
    painter.setBrush(panelFillColor); // 主题变更由 applyTheme() 同步，绘制路径零 QSettings 读取
    painter.drawPath(panel);
    // Apple 式背景材质细化（2026-10-14 裁决）之一：光影渐变（上亮→下沉）——“光从上打下来”的材质层次
    {
        QLinearGradient shade(0, 0, 0, height());
        shade.setColorAt(0.0, Theme::panelGradTop());
        shade.setColorAt(0.5, Qt::transparent);
        shade.setColorAt(0.75, Qt::transparent);
        shade.setColorAt(1.0, Theme::panelGradBottom());
        painter.setBrush(shade);
        painter.drawPath(panel);
    }
    // 之二：细颗粒（材质触感 + 防渐变色带）；1 颗粒 = 1 设备像素（用户复核“再细一点”）
    {
        QBrush grain = grainTextureBrush();
        const qreal dpr = devicePixelRatioF();
        grain.setTransform(QTransform::fromScale(1.0 / dpr, 1.0 / dpr));
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.setBrush(grain);
        painter.drawPath(panel);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    // 之三：顶部受光缘（内侧细高光线，随圆角弯曲；渐变画笔只覆盖顶部数像素 → 两侧自然淡出）
    {
        QPainterPath inner;
        inner.addRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 11, 11);
        QLinearGradient edge(0, 0, 0, 3);
        edge.setColorAt(0.0, Theme::panelTopSheen());
        edge.setColorAt(1.0, Qt::transparent);
        painter.setPen(QPen(QBrush(edge), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(inner);
    }
    painter.setPen(QPen(panelStrokeColor, 1)); // 细描边：在明/暗壁纸上勾勒面板轮廓（随主题切换）
    painter.drawPath(panel);
}

/// 通知前台窗口变化
/// @param hwnd 前台窗口句柄
/// @param source 通知来源, for debug, @b Optional
void Widget::notifyForegroundChanged(HWND hwnd, ForegroundChangeSource source) { // TODO isVisible or AltDown时，关闭前台更新通知
    if (hwnd == this->hWnd()) return;
    // （其实监听前台变化只是为了排序，不需要太精确，可以放松限制）
    // 通过`EVENT_SYSTEM_FOREGROUND`触发时忽略`IsWindowVisible`，因为窗口在创建瞬间可能不可见
    QString path;
    // 单趟复用：过滤通过时已解析出进程路径（pathOut 仅在通过分支写入），不再二次查询（每次前台变化都走这里）
    if (!Util::isWindowAcceptable(hwnd, source == WinEvent, &path)) return;
    // 注：winActiveOrder 的失效清理在 prepareWindowList（每次弹窗时执行，防止无限增长与句柄复用错乱）
    winActiveOrder[path].insert(hwnd, QDateTime::currentDateTime());

    auto sourceStr = QMetaEnum::fromType<ForegroundChangeSource>().valueToKey(source);
    // 注意：不为日志同步求值 getFileDescription——那是 Shell 属性慢查询，而本函数是每次前台变化都会路过
    qDebug() << qUtf8Printable(QString("*ForeWin changed (%1):").arg(sourceStr)) // qUtf8Printable removes quotes ""
            << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd) << path;
} // TODO 控制面板 和 资源管理器 exe是同一个，如何区分图标

/// 通知前台窗口变化（已知进程路径的捷径）：跳过滤镜与进程路径查询——调用点（弹窗条目）已在枚举时过滤并解析过
void Widget::notifyForegroundChanged(HWND hwnd, const QString& knownPath, ForegroundChangeSource source) {
    if (hwnd == this->hWnd()) return;
    if (knownPath.isEmpty()) return; // 与滤镜“空路径窗口不入列表”的语义一致
    winActiveOrder[knownPath].insert(hwnd, QDateTime::currentDateTime());

    auto sourceStr = QMetaEnum::fromType<ForegroundChangeSource>().valueToKey(source);
    qDebug() << qUtf8Printable(QString("*ForeWin changed (%1):").arg(sourceStr))
            << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd) << knownPath;
}

/// collect, filter, sort Windows for presentation（每个窗口独立一项，按各自最近活跃排序）
QList<WindowInfo> Widget::prepareWindowList() {
    QHash<HWND, QString> pathMap; // 枚举时顺带解析进程路径（单趟完成，避免逐窗口重复查询）
    QHash<HWND, QString> classMap; // 枚举时顺带带回类名（省去逐窗口再次 GetClassName）
    const auto list = Util::listValidWindows(&pathMap, &classMap);
    QSet<HWND> validHwnds(list.begin(), list.end());
    // 清理 winActiveOrder 中已失效的 HWND（防无限增长与句柄复用错乱），顺带构建 winOrder（同一趟完成）
    QHash<HWND, QDateTime> winOrder;
    for (auto it = winActiveOrder.begin(); it != winActiveOrder.end();) {
        auto& hwndMap = it.value();
        for (auto hIt = hwndMap.begin(); hIt != hwndMap.end();) {
            if (!validHwnds.contains(hIt.key()))
                hIt = hwndMap.erase(hIt);
            else {
                winOrder.insert(hIt.key(), hIt.value());
                ++hIt;
            }
        }
        if (hwndMap.isEmpty())
            it = winActiveOrder.erase(it);
        else
            ++it;
    }

    QList<WindowInfo> winList;
    for (auto hwnd: list) {
        if (hwnd == this->hWnd()) continue; // skip self
        const auto path = pathMap.value(hwnd); // 路径已在枚举（过滤）时解析，不再重复查询
        if (path.isEmpty()) continue; // TODO 可能需要管理员权限
        winList.append({Util::getWindowTitle(hwnd), classMap.value(hwnd), hwnd, path});
    }

    // 按窗口自身最近活跃时间排序（最新在前；无记录者排后，保持枚举序）
    // stable_sort：无记录者互相等价，必须保持枚举序（与上述“无记录者排后，保持枚举序”的注释承诺一致）
    std::stable_sort(winList.begin(), winList.end(), [&winOrder](const WindowInfo& a, const WindowInfo& b) {
        auto timeA = winOrder.value(a.hwnd);
        auto timeB = winOrder.value(b.hwnd);
        if (timeA.isNull() && timeB.isNull()) return false;
        if (timeA.isValid() && timeB.isValid()) return timeA > timeB;
        return timeA.isValid();
    });
    return winList;
}

bool Widget::prepareListWidget() {
    // 新一轮弹窗交互开始：解除上一轮遗留的"双击屏蔽"（若有），避免其吞掉本轮在同一位置的第一次点击
    Util::disarmClickShield();
    auto winList = prepareWindowList();

    // get screen（窗口定位与自适应都需要，提前获取）
    bool displayOnPrimary = (CFG.getDisplayMonitor() == PrimaryMonitor);
    auto screen = displayOnPrimary ?
                  QGuiApplication::primaryScreen() :
                  QGuiApplication::screenAt(QCursor::pos()); // multi-screen support
    if (!screen && !displayOnPrimary) { // fallback to primary screen
        qWarning() << "Cursor Screen nullptr! Fallback to primary";
        screen = QApplication::primaryScreen();
    }
    if (!screen) {
        qWarning() << "Screen nullptr!";
        sysTray.showMessage("Error", "Screen nullptr!");
        pinned = false;
        hide(); // 列表不可用 ⇒ 弹窗不驻留（否则空面板会留在屏幕上）
        return false;
    }

    // 自适应：应用过多时压缩单元格与图标，保证整行容纳在屏幕宽度内（每次弹窗重算，数量回落自动复原）
    // 基准：默认单元格 80 / 图标 64（四周 8px 边距）；压缩下限：单元格 48 / 图标 32
    if (!winList.isEmpty()) {
        constexpr int CELL_DEFAULT = 80, ICON_DEFAULT = 64, CELL_MIN = 48, ICON_MIN = 32;
        const int count = winList.size();
        const int availableW = screen->availableGeometry().width() - ListWidgetMargin.left() - ListWidgetMargin.right() - 8; // 8px 余量
        const int cell = qBound(CELL_MIN, availableW / count, CELL_DEFAULT);
        const int icon = qBound(ICON_MIN, cell - (CELL_DEFAULT - ICON_DEFAULT), ICON_DEFAULT);
        lw->setGridSize({cell, cell});
        lw->setIconSize({icon, icon});
        lw->setFixedHeight(cell); // 列表高度 = 单元格高度（单行）
        if (cell < CELL_DEFAULT)
            qInfo() << "Adaptive icons:" << count << "apps -> cell" << cell << ", icon" << icon;
    }

    lw->clear();
    for (auto& info: winList) {
        // 图标：统一应用图标（jumbo 预取缓存，锐利）；
        // USE_WINDOW_ICON=true 时优先窗口自身图标（可区分文件夹/头像，但 32px 源图放大偏糊）
        QIcon icon;
        if (USE_WINDOW_ICON) {
            auto pm = Util::getWindowIcon(info.hwnd);
            if (!pm.isNull())
                icon = QIcon(pm);
        }
        if (icon.isNull()) {
            icon = Util::getCachedIcon(info.exePath, info.hwnd);
            // QQ：在应用图标右下角叠加"当前聊天对象头像"角标（同应用多窗口仍可区分）
            if (info.exePath.endsWith("QQ\\bin\\QQ.exe", Qt::CaseInsensitive)) {
                auto overlay = Util::getWindowIcon(info.hwnd);
                if (!overlay.isNull()) {
                    const auto iSize = lw->iconSize();
                    icon = Util::overlayIcon(icon.pixmap(iSize), overlay, {{iSize.width() / 2, iSize.height() / 2}, iSize / 2});
                }
            }
        }
        auto item = new QListWidgetItem(icon, {}); // null != "", which will completely hide text area
        item->setData(Qt::UserRole, QVariant::fromValue(info));
        item->setSizeHint(lw->gridSize()); // 决定了delegate的绘制区域，比grid小的话，paintRect就不居中了，而且update也不及时
//        item->setFlags(item->flags() & ~Qt::ItemIsSelectable); // 不可选中
        lw->addItem(item);
    }

    // 初始选中行：首个 item 是前台窗口时选第二行（直接比对窗口句柄；因为有些情况：选中桌面 并不会产生一个 item）
    // —— 面板宽度自适应与下方 setCurrentRow 共用同一次判定，避免两处重复调用 GetForegroundWindow 产生不一致
    const int initialRow = (winList.size() >= 2 && winList.at(0).hwnd == GetForegroundWindow()) ? 1 : 0;

    // calculate Geometry
    if (auto firstItem = lw->item(0)) {
        auto firstRect = lw->visualItemRect(firstItem);
        auto width = lw->gridSize().width() * lw->count() + (firstRect.x() - lw->frameWidth()); // 一些微小的噼里啪啦修正
        // 宽度夹取：压缩到 CELL_MIN 仍可能超屏（如 1280 逻辑宽 ≈26 窗口起）——对任意 N 兑现
        // “保证整行容纳在屏幕内”承诺：超宽部分截断 + 日志（出屏条目仅键盘循环可达）
        const int maxListW = screen->availableGeometry().width() - ListWidgetMargin.left() - ListWidgetMargin.right();
        if (width > maxListW) {
            const int hidden = lw->count() - qMax(1, maxListW / qMax(1, lw->gridSize().width()));
            qWarning() << "Panel width clamped:" << width << "->" << maxListW
                       << "; items" << lw->count() << "hidden ~" << hidden;
            width = maxListW;
        }
        lw->setFixedWidth(width);

        // move to scrren center
        qDebug() << "Screen:" << screen->name();
        auto lwRect = lw->rect();
        auto thisRect = lwRect.marginsAdded(ListWidgetMargin);

        // macOS 风格：面板宽度至少容纳“初始选中项”的完整标题（标题上限 560 逻辑px；超出部分由省略号处理），
        // 使少量窗口 + 长标题的场景下文字无需截断；列表本身保持原宽、随后统一居中。
        {
            QString initialText = winList.at(initialRow).title;
            if (initialText.isEmpty())
                initialText = Util::getFileDescription(winList.at(initialRow).exePath);
            if (!initialText.isEmpty()) {
                constexpr int kLabelTextCap = 560; // 标题最大展示宽度（逻辑px，约 42 个汉字）
                const QFontMetrics fm(ui->label->font());
                const int wantW = qMin(fm.horizontalAdvance(initialText), kLabelTextCap) + 16; // 左右各留 8px
                const int maxW = screen->availableGeometry().width() - 64; // 屏幕安全余量
                thisRect.setWidth(qMax(thisRect.width(), qMin(wantW, maxW)));
            }
        }

        // macOS 风格：垂直留白近似对称——图标顶边距（上边距 + 图标内缩）与文字底边距对应；
        // 标题字号变大时，底部留白随之增大（每次弹窗按当前字号实时计算）。
        // 用户裁决（2026-10-14）：文字下方到底边的留白较顶部再收窄一点点（顶部与图标-文字距离不动）
        {
            const auto fm = QFontMetrics(ui->label->font());
            const int iconInset = (lw->gridSize().width() - lw->iconSize().width()) / 2;
            const int gapBelowText = ListWidgetMargin.top() + iconInset; // 基准等距值（= 图标顶边距）
            constexpr int kBottomPadTrim = 6; // 底部留白再收窄的微调量（逻辑px；视觉平衡用）
            const int bottomPad = qMax(0, gapBelowText - fm.descent() - kBottomPadTrim); // 可见文字底（基线附近）到面板底 ≈ 基准 - 微调
            thisRect.setHeight(ListWidgetMargin.top() + lw->gridSize().height() + kCellLabelGap + fm.height() + bottomPad);
        }

        thisRect.moveCenter(screen->geometry().center());

        // 水平夹取：标题加宽后仍可能超“可用区”宽度——贴边而非出屏（别用 geometry()：竖排任务栏会重叠）
        const auto availGeo = screen->availableGeometry();
        if (thisRect.left() < availGeo.left())
            thisRect.moveLeft(availGeo.left());
        else if (thisRect.right() > availGeo.right())
            thisRect.moveRight(availGeo.right());

        //region Fixed in Qt6, see commit [b927ee4b]
        //      !!!WARNING: 对于多屏幕，直接使用setGeometry or move会报错(QWindowsWindow::setGeometry: Unable to set geometry) & size显示不正确！
        //      报错时机为：从一个屏幕hide，再在另一个屏幕show; 第二次在同一个屏幕show，则正常
        //      size显示不正确不能忍，遂改用WinAPI
        //endregion
        this->setGeometry(thisRect);

        //region Fixed in Qt6, see commit [b927ee4b]
        //this->windowHandle()->setScreen(screen); // 若首次显示是在副屏，会导致size显示错误（如果没有这行）
        // `toNativePixels`是针对Point的，会根据屏幕原点进行位移
        // 对于其他类型（如Size），直接乘以`QHighDpiScaling::factor(screen)`即可
        //auto physicalPos = QHighDpi::toNativePixels(thisRect.topLeft(), screen);
        // 如果用SetWindowPos的话要注意加上`SWP_NOACTIVATE`，否则焦点有问题，没错，NoActive反而是Active (focus)的
        //SetWindowPos(hWnd(), nullptr, physicalPos.x(), physicalPos.y(), 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
        // !!!NOTE: 用WinAPI控制size貌似有问题，在图标增减的时候，无法正确调整Width，离子谱；只能用resize
        // 1. × 猜想是showNormal()恢复了原有的size，导致resize无效；但是改成show()也不行
        // 2. 猜想是隐藏状态改变size无效？（不科学吧），但是在`SetWindowPos`前show()好像会好一点（第二次显示调整size成功）aaa
        //this->resize(thisRect.size());
        //endregion

        lwRect.moveLeft((this->width() - lwRect.width()) / 2); // 水平居中（面板可能因标题加宽）
        lwRect.moveTop(ListWidgetMargin.top());                // 顶部固定：底部留白按“文字等距”规则确定
        lw->move(lwRect.topLeft());
    } else {
        // 无可切换窗口：确保弹窗不驻留（清空后的空面板不能留在屏幕上）
        pinned = false;
        hide();
        return false;
    }

    // set current item（initialRow 已在上方统一判定）
    if (lw->count() >= 1)
        lw->setCurrentRow(initialRow); //! 首次显示时，该行特别耗时：472ms

    return true;
}

bool Widget::requestShow() { // TODO 当前台是开始菜单（Win）时，会导致显示 但无法操控
    pinned = false;
    return prepareListWidget() && forceShow();
}

/// pinned 模式呼出：松开按键后保持显示，不随 Alt 释放而切换/隐藏
/// 期间可用 Tab/`/方向键/Vim 键选择，Enter/空格/点击 确认切换，Esc 取消
/// 方案A：Alt+Tab 与 Ctrl+Alt+Tab 均走此路径
void Widget::requestShowPinned() {
    if (pinned && this->isVisible() && !this->isMinimized()) {
        // 已在 pinned 显示中：重复触发不确认（钩子已对自动重复去重，这里防御），
        // 刷新列表继续选择；若被其他窗口抢走了焦点/前台，重新置前（不刷新则可能被压到后面）
        if (!prepareListWidget()) {
            pinned = false;
            return;
        }
        if (!isForeground())
            forceShow(); // 抢回前台（showMinimized/showNormal 组合）
        return;
    }
    pinned = true;
    if (!prepareListWidget()) {
        pinned = false;
        return;
    }
    forceShow();
}

/// 单击图标：选中目标并立即切换（鼠标确认路径；事件过滤器与 pressed 信号兜底共用）
void Widget::handleIconClick(QListWidgetItem* item) {
    if (!this->isVisible()) return;
    if (lw->currentItem() != item)
        lw->setCurrentItem(item); // 供 switchToCurrentItem 取目标
    Util::armClickShield(QGuiApplication::styleHints()->mouseDoubleClickInterval() + 100);
    qInfo() << "Click switch -> row" << lw->row(item);
    switchToCurrentItem(); // 立即切换并隐藏
}

/// 中键单击图标：关闭该窗口并移除条目（弹窗保持显示，可连续关闭多个；全部关完自动收起）
void Widget::closeWindowItem(QListWidgetItem* item) {
    if (lw->row(item) < 0) return; // 防御：条目已移除（兜底路径与过滤器竞态时不重复处理）
    auto info = item->data(Qt::UserRole).value<WindowInfo>();
    if (info.hwnd) {
        qInfo() << "Middle-click close ->" << info;
        PostMessage(info.hwnd, WM_CLOSE, 0, 0); // 优雅关闭请求：应用可正常弹保存确认；对更高权限窗口可能被系统拦截
    }
    delete lw->takeItem(lw->row(item)); // 立即移出图标（若移除的是当前项，Qt 自动改选并触发 currentItemChanged → 描述自动更新）
    if (lw->count() == 0) { // 已无可切换窗口：整体收起
        pinned = false;
        hide();
    }
}

/// Ctrl+W：关闭当前选中项对应的窗口（无选中时无操作；行为与鼠标中键一致）
void Widget::closeCurrentItem() {
    if (auto* item = lw->currentItem())
        closeWindowItem(item);
}

/// 切换到当前选中项对应的窗口并隐藏（Alt 释放与 pinned 确认共用路径）
void Widget::switchToCurrentItem() {
    if (auto item = lw->currentItem()) {
        auto info = item->data(Qt::UserRole).value<WindowInfo>();
        if (info.hwnd) {
            Util::switchToWindow(info.hwnd);
            qInfo() << "Switch to" << info;
        }
    }
    pinned = false; // 已离开 pinned 状态（确认切换后）
    hide(); //! must hide after active target window, or focus may fallback to prev foreground window (like 网易云音乐)
}

/// 循环选择（forward=true 下一个 / false 上一个）——供 keyPressEvent 与 eventFilter 共用，
/// 不依赖列表控件的原生键盘处理（钩子转发的合成事件与被控件消费的按键都从这里走）
void Widget::cycleSelection(bool forward) {
    const int N = lw->count();
    if (N == 0) return;
    int i = lw->currentRow();
    if (i < 0) i = forward ? -1 : 0; // 无选中时：下一个 → 首项；上一个 → 末项
    lw->setCurrentRow((i + (forward ? 1 : -1) + N) % N);
}

/// sort Windows of [Group specified by exePath], by active order (latest first)
void Widget::sortGroupWindows(QList<HWND>& windows, const QString& exePath) {
    auto activeOrdMap = winActiveOrder.value(exePath);
    if (activeOrdMap.isEmpty()) return;
    // sort by active order（stable：无记录者保持既有次序）
    std::stable_sort(windows.begin(), windows.end(), [&activeOrdMap](HWND a, HWND b) {
        return activeOrdMap.value(a) > activeOrdMap.value(b); // default value if not found
    }); // 失效 HWND 的剔除由 prepareWindowList 统一执行（此处排序容忍映射中存在失效项）
}

/// group by exePath, sort by active order (last active first)
QList<HWND> Widget::buildGroupWindowOrder(const QString& exePath) {
    auto windows = Util::listValidWindows(exePath); // filter by path
    sortGroupWindows(windows, exePath);
    return windows;
}

bool Widget::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == lw || watched == lw->viewport()) && event->type() == QEvent::MouseButtonPress) {
        // 单击=直接切换（用户裁决；双击效果已废除）：左键点在图标上即切换并隐藏。
        // 接收者可能是 listWidget 或其 viewport（鼠标事件的实际接收者是后者）；
        // 坐标统一用屏幕坐标映射回 viewport 坐标系，不依赖事件来自哪个对象
        // 双击的第二下由"点击屏蔽"吞掉（Util::armClickShield），避免其落到切换后的窗口上造成误触
        // 中键（滚轮中键）单击图标 = 关闭该窗口（优雅关闭；不切换、不隐藏弹窗）
        auto* me = static_cast<QMouseEvent*>(event);
        if (this->isVisible() && (me->button() == Qt::LeftButton || me->button() == Qt::MiddleButton)) {
            if (auto* item = lw->itemAt(lw->viewport()->mapFromGlobal(me->globalPosition().toPoint()))) {
                if (me->button() == Qt::MiddleButton)
                    closeWindowItem(item);
                else
                    handleIconClick(item);
                return true; // 消费本次按下
            }
        }
    }
    if ((watched == lw || watched == lw->viewport()) && event->type() == QEvent::MouseMove) {
        // 悬停实时选中：鼠标移到某图标上，选中项立即切到该窗口（currentItemChanged → 底部描述实时更新）
        if (this->isVisible()) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (auto* item = lw->itemAt(lw->viewport()->mapFromGlobal(me->globalPosition().toPoint()))) {
                if (lw->currentItem() != item)
                    lw->setCurrentItem(item);
            }
        }
        return false; // 不消费：保持原生悬停/绘制行为
    }
    if (watched == lw && event->type() == QEvent::KeyPress) {
        // 焦点在列表控件时，确认/取消键不会冒泡到 Widget::keyPressEvent，在此拦截
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        const auto key = keyEvent->key();
        if (this->isVisible() && (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space)) {
            switchToCurrentItem();
            return true; // 不再传给列表（空格默认会触发 item 激活/选中变化）
        }
        if (this->isVisible() && key == Qt::Key_Escape) {
            pinned = false;
            hide();
            return true;
        }
        if (this->isVisible() && key == Qt::Key_W && keyEvent->modifiers() == Qt::ControlModifier) {
            // Ctrl+W：关闭当前选中的窗口（与鼠标中键等效）
            closeCurrentItem();
            return true;
        }
        // 兜底：焦点在列表控件时，Tab/Backtab/` 会被控件消费（焦点导航/键盘搜索），
        // 不会冒泡到 Widget::keyPressEvent——在此直接执行循环选择（直调比重发事件可靠，
        // 避免被 QWidget::event 的 Tab 焦点切换逻辑二次拦截；正常路径由键盘钩子直接投递）
        if (this->isVisible() && !(keyEvent->modifiers() & Qt::ControlModifier) &&
            (key == Qt::Key_Tab || key == Qt::Key_Backtab || key == Qt::Key_QuoteLeft)) {
            const bool isShiftPressed = (keyEvent->modifiers() & Qt::ShiftModifier) || key == Qt::Key_Backtab;
            cycleSelection(key == Qt::Key_QuoteLeft ? isShiftPressed : !isShiftPressed);
            return true;
        }
    }
    if (watched == lw && event->type() == QEvent::Wheel) {
        auto* wheelEvent = static_cast<QWheelEvent*>(event);
        // 命中判定用 viewport 坐标（与点击路径同一纪律）：滚轮事件可能以 lw 为接收者，
        // 其 position() 为 lw 坐标，与 itemAt() 所需 viewport 坐标相差一个 frameWidth
        auto cursorPos = lw->viewport()->mapFromGlobal(wheelEvent->globalPosition().toPoint());
        if (auto item = lw->itemAt(cursorPos)) {
            if (lw->currentItem() != item)
                lw->setCurrentItem(item);
            auto info = item->data(Qt::UserRole).value<WindowInfo>();
            if (!info.hwnd || !IsWindow(info.hwnd)) return false; // 防御：列表构建后窗口可能已关闭（死句柄的 IsIconic 未定义）

            // 滚轮语义：↑=该窗口置顶预览（不激活）；↓=最小化该窗口（原"同应用窗口轮换"随合并移除）
            bool isRollUp = wheelEvent->angleDelta().x() > 0; // ListWidget的方向改成了从左到右，所以滚轮方向从y()变成x()了；伪造的 keyPress 事件（keyPressEvent 的 Up/Down 分支）同样使用 x 分量，与真实滚轮保持一致
            if (isRollUp) {
                Util::bringWindowToTop(info.hwnd, this->hWnd()); // without activate
                notifyForegroundChanged(info.hwnd, info.exePath, Inner); // 记录为最近使用（与旧滚轮行为一致；路径取自条目，免二次查询）
            } else if (!IsIconic(info.hwnd)) {
                ShowWindow(info.hwnd, SW_SHOWMINNOACTIVE); // minimize
            }
            qDebug() << "Wheel" << isRollUp << info.title << info.hwnd;

            return true; // stop propagation
        }
    }
    return false;
}

/// `forward`: true for restore, false for minimize
void Widget::rotateTaskbarWindowInGroup(const QString& exePath, bool forward, int windows) {
    static constexpr auto mouseEvent = [](DWORD flag) { // 文件内共享：timer 回调 lambda 也要用，不能是块作用域
        // 已迁移至 SendInput（mouse_event 为 legacy API）；不设 MOVE 标志 = 于当前光标位置注入按钮事件，语义与原来一致
        INPUT input = {.type = INPUT_MOUSE};
        input.mi.dwFlags = flag;
        SendInput(1, &input, sizeof(INPUT));
    };
    qDebug() << "(Taskbar)Wheel on:" << exePath << forward << windows;
    if (exePath.isEmpty()) return;
    if (!windows) { // 程序没有打开的窗口，处于关闭状态; 若不拦截，可能造成错误窗口被触发：explorer.exe -> msedge.exe
        qDebug() << "No window for this app";
        return;
    }

    // 原函数内 static 状态已改为成员变量（taskbarLastPath/taskbarLastHwnd/isLastTaskbarForward），避免悬垂句柄污染
    if (taskbarLastPath != exePath) {
        taskbarLastPath = exePath;
        groupWindowOrder.clear();
    }
    if (groupWindowOrder.isEmpty()) {
        groupWindowOrder = buildGroupWindowOrder(exePath);
        taskbarLastHwnd = nullptr;
    }

    if (groupWindowOrder.isEmpty()) {
        qCritical() << "No window in group!" << exePath;
        // 有些软件的窗口是由子进程创建的，如 steam.exe -> steamwebhelper.exe (持有窗口)
        // 但是在任务栏只能获取到父进程steam.exe
        // 这种情况下，需要查找其子进程的路径
        auto childPaths = Util::getChildProcessPaths(exePath);
        if (childPaths.isEmpty()) return;
        if (childPaths.size() == 1) {
            qDebug() << "Try to switch to child process:" << childPaths.first();
            groupWindowOrder = buildGroupWindowOrder(childPaths.first());
        } else {
            // 如果有多个子进程路径，就根据validWindows过滤
            qWarning() << "!Multiple child processes:" << childPaths;
            QSet<QString> validPaths;
            QHash<HWND, QString> pathMap; // 枚举时已解析路径
            // If range-initializer returns a temporary, its lifetime is extended until the end of the loop
            for (auto hwnd: Util::listValidWindows(&pathMap)) {
                if (auto path = pathMap.value(hwnd); !path.isEmpty())
                    validPaths.insert(path.toLower());
            }
            for (auto& path: childPaths) {
                if (validPaths.contains(path.toLower())) {
                    qDebug() << "Try to switch to valid child process:" << path;
                    groupWindowOrder = buildGroupWindowOrder(path);
                    break;
                }
            }
        }
        // TODO 有可能a进程开启b进程之后，a就关闭了，他俩也没有真的父子关系
        //  例如：ksolaunch.exe -> wps.exe
        //  此时只能通过File Description来匹配，均为“WPS Office”
        if (groupWindowOrder.isEmpty()) { // 无力回天
            qWarning() << "No valid window found for" << exePath << "and its child processes"; // 原日语吐槽改为统一日志级别
            return;
        }
    }

    HWND hwnd = nullptr;
    if (!taskbarLastHwnd) {
        hwnd = groupWindowOrder.first();
        if (forward && hwnd == GetForegroundWindow()) // 如果first是前台窗口且forward，则轮换下一个
            hwnd = rotateWindowInGroup(groupWindowOrder, hwnd, true);
    } else {
        if (isLastTaskbarForward == forward)
            hwnd = rotateWindowInGroup(groupWindowOrder, taskbarLastHwnd, forward);
        else
            hwnd = taskbarLastHwnd;
    }
    isLastTaskbarForward = forward;

    // 使用前校验句柄有效性，避免对已关闭窗口操作
    if (hwnd && !IsWindow(hwnd))
        hwnd = nullptr;
    if (!hwnd) {
        taskbarLastHwnd = nullptr;
        return;
    }

    if (forward) {
        if (windows == 1) { // 由于过滤的存在，groupWindowOrder.size() 不一定等于 windows(真实窗口数量)
            // 单窗口情况下，模拟点击呼出，是最保险的
            if ((hwnd != GetForegroundWindow() || IsIconic(hwnd))) { // 若采用SW_SHOWMINNOACTIVE, 则前台窗口不会变化，可能为刚刚最小化的窗口
                // 注入前复核光标仍在任务栏上：本流程含排队/延时（20ms~200ms），光标若已移开，
                // 合成点击会落到其它窗口或桌面上
                if (Util::isTaskbarWindow(Util::topWindowFromPoint(Util::getCursorPos()))) {
                    mouseEvent(MOUSEEVENTF_LEFTDOWN);
                    mouseEvent(MOUSEEVENTF_LEFTUP);
                    qApp->processEvents();
                    qDebug() << "(Taskbar)Switch by click";
                } else {
                    qDebug() << "(Taskbar)Switch skipped: cursor no longer on taskbar";
                }
            }
        } else {
            // 在TaskListThumbnailWnd显示的情况下restore window会导致预览实时刷新，导致卡顿和闪烁
            // 隐藏TaskListThumbnailWnd也无效，会自动show
            // DwmSetWindowAttribute[DWMWA_FORCE_ICONIC_REPRESENTATION, DWMWA_DISALLOW_PEEK], 效果都不好，还是会刷新闪烁

            // 只能采用偷鸡hack，按住左键的情况下，预览窗口会消失
            if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                qDebug() << "(Taskbar)#Press LButton";
                mouseEvent(MOUSEEVENTF_LEFTDOWN);
                // 由于本程序hook了mouse，所以必须处理全局鼠标事件（in事件循环）
                QTimer::singleShot(20, this, [hwnd]() {
                    Util::switchToWindow(hwnd, true); // TODO thumbnail隐藏之前 不要switch，并且block滚轮 防止闪烁卡顿
                });
            } else
                Util::switchToWindow(hwnd, true);

            static QTimer* timer = [this]() {
                auto* t = new QTimer;
                t->setSingleShot(true);
                t->setInterval(200);
                // TODO cursor移动后立即释放 防止拖拽
                t->callOnTimeout(this, [this]() {
                    mouseEvent(MOUSEEVENTF_LEFTUP);
                    qDebug() << "(Taskbar)#Release LButton";

                    // 鼠标点击thumbnail之后，其获取焦点，此时若焦点在其窗口组成员中，thumbnail就不会隐藏，这是Windows机制
                    // 只能通过将焦点转移到Taskbar使其隐藏
                    // 直接 HIDE thumbnail 不太行，会导致之后restore窗口时 thumbnail刷新 + 窗口闪烁，闪瞎了
                    QTimer::singleShot(100, this, []() {
                        // 等待thumbnail显示
                        if (HWND thumbnail = Util::getCurrentTaskListThumbnailWnd(); IsWindowVisible(thumbnail)) {
                            if (HWND taskbar = FindWindow(L"Shell_TrayWnd", nullptr))
                                Util::switchToWindow(taskbar, true);
                        }
                    });
                });
                return t;
            }();
            timer->stop();
            timer->start();
        }
        qDebug() << "(Taskbar)Switch to" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
    } else {
        if (auto normal = rotateNormalWindowInGroup(groupWindowOrder, hwnd, false)) { // skip minimized
            if (normal != hwnd)
                qDebug() << "(Taskbar)Skip minimized" << hwnd << "->" << normal;
            hwnd = normal;
            ShowWindow(hwnd, SW_MINIMIZE); // SW_MINIMIZE 会让焦点自动回落到下一个窗口
            // 当所有窗口隐藏后，getElementUnderMouse() 会变成"CEF-OSC-WIDGET"，但是焦点和前台窗口并不是他，离谱
            // 此时Automation对鼠标下任务栏Element的判定会出错，solution为手动变焦到任务栏（见TaskbarWheelHooker.cpp）
            // SW_SHOWMINNOACTIVE不会切换焦点，即便本窗口已经最小化，但仍然持有焦点；但这不是合理的行为，同时会让QQ Follower反复弹出
            qDebug() << "(Taskbar)Minimize" << hwnd << Util::getWindowTitle(hwnd) << Util::getClassName(hwnd);
        }
    }

    taskbarLastHwnd = hwnd;
}

/// select next(forward)(older) or prev window in group<br>
/// Do nothing, but select HWND
HWND Widget::rotateWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    const auto N = windows.size();
    if (N == 0) return nullptr; // 空列表守卫，避免 first()/取模 UB
    if (N == 1) return windows.first();
    for (int i = 0; i < N; i++) {
        if (windows.at(i) == current) {
            auto next_i = forward ? (i + 1) : (i - 1);
            auto next = windows.at((next_i + N) % N);
            return next;
        }
    }
    return nullptr;
}

/// Select next (including `current`) normal (!minimized) window in group<br>
/// return nullptr if all minimized
HWND Widget::rotateNormalWindowInGroup(const QList<HWND>& windows, HWND current, bool forward) {
    if (windows.isEmpty()) return nullptr; // 空列表守卫
    if (current && !IsWindow(current)) current = nullptr; // 死句柄防御：IsIconic 对死句柄返回值未定义
    for (int i = 0; IsIconic(current) && i < windows.size(); i++) // skip minimized
        current = rotateWindowInGroup(windows, current, forward);
    return IsIconic(current) ? nullptr : current;
}

void Widget::clearGroupWindowOrder() {
    groupWindowOrder.clear();
}
