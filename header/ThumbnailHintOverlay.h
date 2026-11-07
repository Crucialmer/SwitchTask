#ifndef SWITCHTASK_THUMBNAILHINTOVERLAY_H
#define SWITCHTASK_THUMBNAILHINTOVERLAY_H

#include <QWidget>

class QPropertyAnimation;

/// 缩略图"被遮挡"提示胶囊（macOS 26 风格）
///
/// 为什么必须是独立顶层窗：DWM 缩略图由 DWM 合成在目标窗口的自绘内容与子控件之上，
/// 在面板里画的文字会被缩略图盖住（面板中"窗口已最小化"的文案只在没有缩略图时才可见）。
/// 所以提示只能另开一个窗口，压在面板与缩略图之上。
///
/// 输入完全穿透（WindowTransparentForInput + WA_TransparentForMouseEvents）：
/// 点击由 Widget 命中矩形后处理，既不抢焦点，也不会打断列表的悬停选中。
class ThumbnailHintOverlay : public QWidget {
    Q_OBJECT

public:
    explicit ThumbnailHintOverlay(QWidget* owner = nullptr);

    /// 胶囊建议尺寸（按文案与字号计算，供 Widget 摆放与命中判定共用）
    QSize desiredSize() const;

    /// 在全局坐标 globalTopLeft 处显示（带 120ms 淡入；已显示时只更新位置与配色）
    void showAt(const QPoint& globalTopLeft, bool dark);

    /// 120ms 淡出后隐藏
    void dismiss();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPropertyAnimation* m_fade = nullptr;
    bool m_dark = true;
    bool m_fadingOut = false;
};

#endif // SWITCHTASK_THUMBNAILHINTOVERLAY_H
