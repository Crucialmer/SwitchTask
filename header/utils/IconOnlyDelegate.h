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
    qreal selectionGrow = 1.0; // 选中项放大动效进度 0~1（由 Widget 的动画驱动；macOS 风格）。
                               // 默认 1.0 = 放大定格态：动画只负责“收缩→放大”过渡，
                               // 即使动画因环境原因未执行，选中项也保持放大视觉（不依赖动画完成）

public:
    /// 两个颜色为必填：唯一真源是 Theme（构造点直接传 Theme::selectionFill()/hoverFill()；
    /// 不再提供默认实参——默认值若与 Theme 漂移会形成第二来源，且实际从未生效过（applyTheme 随即覆盖））
    explicit IconOnlyDelegate(QObject* parent, QColor selectedColor, QColor hoverColor)
        : QStyledItemDelegate(parent), selectedColor(selectedColor), hoverColor(hoverColor) {}

    /// 主题切换时更新两色（选中底/悬停底）
    void setColors(const QColor& selected, const QColor& hover) {
        selectedColor = selected;
        hoverColor = hover;
    }

    /// 动画步进（调用方随后触发 viewport 重绘）
    void setSelectionGrow(qreal grow) { selectionGrow = grow; }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};


#endif //WIN_SWITCHER_ICONONLYDELEGATE_H
