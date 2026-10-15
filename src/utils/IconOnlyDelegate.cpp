#include "utils/IconOnlyDelegate.h"
#include "widget.h"
#include "utils/Util.h"
#include <QPen>

// 方圆角（squircle）路径由 Util::squirclePath 提供（用于选中底/悬停底绘制；超椭圆指数 n=5）

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    // macOS 风格：连续曲率平滑圆角（与外面板保持相同几何语言，内缩防抗锯齿裁切）
    const auto rectF = QRectF(option.rect).adjusted(2, 2, -2, -2);
    const auto squircle = Util::smoothRoundedRectPath(rectF, 14);

    if (option.state & QStyle::State_Selected) {
        painter->save();
        // 触觉弹性动效作用于高亮胶囊，而非位图图标本身，杜绝图标非整数倍缩放导致的画质发糊
        if (selectionGrow < 0.999 && selectionGrow > 0.001) {
            const auto c = rectF.center();
            painter->translate(c);
            const qreal s = 0.94 + 0.06 * selectionGrow;
            painter->scale(s, s);
            painter->translate(-c);
        }
        // 选中态：macOS 风格中性光晕白胶囊 + 1px 细高光轮廓
        const QColor stroke = selectedStrokeColor.isValid() ? selectedStrokeColor : QColor(255, 255, 255, 75);
        painter->setPen(QPen(stroke, 1));
        painter->setBrush(selectedColor);
        painter->drawPath(squircle);
        painter->restore();
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(hoverColor);
        painter->drawPath(squircle);
    }

    // 居中绘制图标：macOS 原则——图标保持 1:1 原生像素绝对清晰锐利
    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect iconRect{{}, option.decorationSize}; // QListWidget::iconSize()
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
    }
}
