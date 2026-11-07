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

public:
    explicit IconOnlyDelegate(QObject* parent, QColor selectedColor, QColor hoverColor, QColor selectedStroke = {})
        : QStyledItemDelegate(parent), selectedColor(selectedColor), hoverColor(hoverColor), selectedStrokeColor(selectedStroke) {}

    /// 主题切换时更新三色（选中底/悬停底/选中边缘描边）
    void setColors(const QColor& selected, const QColor& hover, const QColor& selectedStroke = {}) {
        selectedColor = selected;
        hoverColor = hover;
        selectedStrokeColor = selectedStroke;
    }


    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
