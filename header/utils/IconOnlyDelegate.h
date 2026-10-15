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
    QColor selectedStrokeColor;
    qreal selectionGrow = 1.0; // 选中胶囊弹性动效进度 0~1（由 Widget 的动画驱动；macOS 风格）

public:
    explicit IconOnlyDelegate(QObject* parent, QColor selectedColor, QColor hoverColor, QColor selectedStroke = {})
        : QStyledItemDelegate(parent), selectedColor(selectedColor), hoverColor(hoverColor), selectedStrokeColor(selectedStroke) {}

    /// 主题切换时更新三色（选中底/悬停底/选中边缘描边）
    void setColors(const QColor& selected, const QColor& hover, const QColor& selectedStroke = {}) {
        selectedColor = selected;
        hoverColor = hover;
        selectedStrokeColor = selectedStroke;
    }

    /// 动画步进（调用方随后触发 viewport 重绘）
    void setSelectionGrow(qreal grow) { selectionGrow = grow; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
