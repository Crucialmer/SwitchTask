#ifndef WIN_SWITCHER_ICONONLYDELEGATE_H
#define WIN_SWITCHER_ICONONLYDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QIcon>
#include <QColor>

/// Icon Only Mode for QListWidget — macOS 26 Liquid Glass 聚光透镜自绘 Delegate
class IconOnlyDelegate : public QStyledItemDelegate {
    QColor selectedTopColor;
    QColor selectedBottomColor;
    QColor selectedStrokeColor;
    QColor selectedHighlightColor;
    QColor hoverColor;
    QColor hoverStrokeColor;

public:
    explicit IconOnlyDelegate(QObject* parent,
                              QColor selectedTop,
                              QColor selectedBottom,
                              QColor selectedStroke,
                              QColor selectedHighlight,
                              QColor hoverFill,
                              QColor hoverStroke)
        : QStyledItemDelegate(parent),
          selectedTopColor(selectedTop),
          selectedBottomColor(selectedBottom),
          selectedStrokeColor(selectedStroke),
          selectedHighlightColor(selectedHighlight),
          hoverColor(hoverFill),
          hoverStrokeColor(hoverStroke) {}

    /// 主题切换时一次性推送全部颜色（paint 热路径零配置查询）
    void setColors(const QColor& selectedTop,
                   const QColor& selectedBottom,
                   const QColor& selectedStroke,
                   const QColor& selectedHighlight,
                   const QColor& hoverFill,
                   const QColor& hoverStroke) {
        selectedTopColor = selectedTop;
        selectedBottomColor = selectedBottom;
        selectedStrokeColor = selectedStroke;
        selectedHighlightColor = selectedHighlight;
        hoverColor = hoverFill;
        hoverStrokeColor = hoverStroke;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
