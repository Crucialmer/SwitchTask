#include "utils/IconOnlyDelegate.h"
#include "widget.h"
#include "utils/Util.h"
#include <QPen>

// 方圆角（squircle）路径由 Util::squirclePath 提供（用于选中底/悬停底绘制；超椭圆指数 n=5）

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform); // 窗口图标源图较小（通常 32px），放大绘制时平滑处理

    // macOS 风格：选中项放大动效——仅“图标”随动画等比放大；选中底块恒为单元格大小，
    // 确保任何状态下都不溢出单元格（不裁切、不与相邻项重叠、不挤压底部标签）
    // 选择态即"当前项"（单选中列表），直接取绘制状态，免去向上游视图探取 currentIndex 的耦合
    constexpr qreal kGrowMax = 0.15; // 图标最大放大比例（1 + 0.15；64px 图标放大后约 74px，仍完全落在 80px 单元格内）
    const bool isCurrent = option.state & QStyle::State_Selected;
    const qreal grow = (isCurrent && selectionGrow > 0.001) ? (1.0 + kGrowMax * selectionGrow) : 1.0;

    painter->setPen(Qt::NoPen); //取消边框
    // option.rect.size() == QListWidgetItem::sizeHint()
    if (option.state & QStyle::State_Selected) {
        // 选中态：中性半透明方圆角底（macOS 风，颜色随深浅色主题；用户裁决 2026-10-14 C 方案）——
        // 无描环，选中提示由图标放大动效与底部标题承担
        painter->setBrush(selectedColor);
        painter->drawPath(Util::squirclePath(QRectF(option.rect).adjusted(1, 1, -1, -1))); // 内缩 1px 防边缘抗锯齿被裁
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(hoverColor);
        painter->drawPath(Util::squirclePath(option.rect));
    }

    // 居中绘制图标（选中项按动画进度围绕单元格中心放大）
    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        painter->save();
        if (grow > 1.0) {
            const auto c = option.rect.center();
            painter->translate(c);
            painter->scale(grow, grow);
            painter->translate(-c);
        }
        QRect iconRect{{}, option.decorationSize}; // QListWidget::iconSize()
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
        painter->restore();
    }

    // 每个窗口独立展示，无"窗口数量"角标
}
