#include "utils/IconOnlyDelegate.h"
#include "utils/Util.h"
#include <QLinearGradient>
#include <QPen>

// 方圆角（squircle）路径由 Util::squirclePath 提供（归一化尺寸缓存；超椭圆指数 n=5）

void IconOnlyDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);

    if (option.state & QStyle::State_Selected) {
        // macOS 26 Liquid Glass 选中透镜：垂直玻璃光影渐变 + 顶部内高光弧 + 纤细折射描边
        const QRectF lensRect = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
        const QPainterPath lensPath = Util::squirclePath(lensRect);

        QLinearGradient fillGrad(lensRect.topLeft(), lensRect.bottomLeft());
        fillGrad.setColorAt(0.0, selectedTopColor);
        fillGrad.setColorAt(1.0, selectedBottomColor);
        painter->setPen(Qt::NoPen);
        painter->setBrush(fillGrad);
        painter->drawPath(lensPath);

        // 透镜顶部内切高光（模拟液态玻璃聚光折射）
        if (selectedHighlightColor.alpha() > 0) {
            const QRectF innerRect = lensRect.adjusted(1.0, 1.0, -1.0, -1.0);
            const QPainterPath innerPath = Util::squirclePath(innerRect);
            QLinearGradient highlightGrad(innerRect.topLeft(), QPointF(innerRect.left(), innerRect.top() + innerRect.height() * 0.42));
            highlightGrad.setColorAt(0.0, selectedHighlightColor);
            highlightGrad.setColorAt(1.0, Qt::transparent);
            painter->setPen(QPen(QBrush(highlightGrad), 1.2));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(innerPath);
        }

        // 外缘细腻玻璃高光轮廓
        if (selectedStrokeColor.alpha() > 0) {
            painter->setPen(QPen(selectedStrokeColor, 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(lensPath);
        }
    } else if (option.state & QStyle::State_MouseOver) {
        // 悬停态：轻通透玻璃底 + 极细边缘光
        const QRectF hoverRect = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);
        const QPainterPath hoverPath = Util::squirclePath(hoverRect);
        painter->setBrush(hoverColor);
        painter->setPen(hoverStrokeColor.alpha() > 0 ? QPen(hoverStrokeColor, 1.0) : Qt::NoPen);
        painter->drawPath(hoverPath);
    }

    // 居中绘制图标：macOS 原则——图标保持 1:1 原生像素绝对清晰锐利
    auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
    if (!icon.isNull()) {
        QRect iconRect{{}, option.decorationSize}; // QListWidget::iconSize()
        iconRect.moveCenter(option.rect.center());
        icon.paint(painter, iconRect);
    }
}
