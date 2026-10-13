#ifndef WIN_SWITCHER_ICONONLYDELEGATE_H
#define WIN_SWITCHER_ICONONLYDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QIcon>
#include <QColor>

/// Icon Only Mode for QListWidget
class IconOnlyDelegate : public QStyledItemDelegate {
    QColor selectedColor;
    QColor hoverColor;
    QColor ringColor;
    qreal selectionGrow = 1.0; // 选中项放大动效进度 0~1（由 Widget 的动画驱动；macOS 风格）。
                               // 默认 1.0 = 放大定格态：动画只负责“收缩→放大”过渡，
                               // 即使动画因环境原因未执行，选中项也保持放大视觉（不依赖动画完成）

public:
    explicit IconOnlyDelegate(QObject* parent = nullptr,
                              QColor selectedColor = QColor(162, 210, 255, 225), // 选中：马卡龙·婴儿蓝 #A2D2FF（高不透明度，明/暗底色上均清晰）
                              QColor hoverColor = QColor(255, 255, 255, 36),     // 悬停：轻白光晕（与选中态层级分明）
                              QColor ringColor = QColor(255, 255, 255, 140))     // 选中描边：细化边界增强对比
        : QStyledItemDelegate(parent), selectedColor(selectedColor), hoverColor(hoverColor),
          ringColor(ringColor) {}

    /// 主题切换时更新三色（选中底/悬停底/描边）
    void setColors(const QColor& selected, const QColor& hover, const QColor& ring) {
        selectedColor = selected;
        hoverColor = hover;
        ringColor = ring;
    }

    /// 动画步进（调用方随后触发 viewport 重绘）
    void setSelectionGrow(qreal grow) { selectionGrow = grow; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
