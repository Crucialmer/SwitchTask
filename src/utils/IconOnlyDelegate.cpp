#include "utils/IconOnlyDelegate.h"
#include "widget.h"
#include "utils/Util.h"
#include <QPen>

// 方圆角（squircle）路径由 Util::squirclePath 提供（用于选中底/悬停底绘制；超椭圆指数 n=5）

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->setPen(Qt::NoPen); // 取消描边，消除硬折痕感，呈现饱满连续方圆角
    if (option.state & QStyle::State_Selected) {
        // 选中态：连续曲率方圆角底（n=5 超椭圆方程，全方位圆润饱满、零切角硬折痕）
        painter->setBrush(selectedColor);
        painter->drawPath(Util::squirclePath(QRectF(option.rect).adjusted(1, 1, -1, -1)));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setBrush(hoverColor);
        painter->drawPath(Util::squirclePath(option.rect));
    }

    // 居中绘制图标：macOS 原则——图标保持 1:1 原生像素绝对清晰锐利
    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect iconRect{{}, option.decorationSize}; // QListWidget::iconSize()
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
    }
}
