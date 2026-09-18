#pragma once
#include <QWidget>
#include <Qt>

namespace SketchCAD {

class GraphicsView;

// 绘图板标尺：水平标尺（顶部）与垂直标尺（左侧）。
// 通过读取 GraphicsView 的 zoom/pan 实时计算可见世界坐标范围，
// 自适应选择"漂亮"的刻度间距（1/2/5/10/20/50/… mm），随缩放与平移联动。
class RulerWidget : public QWidget {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit RulerWidget(GraphicsView* view, Orientation ori, QWidget* parent = nullptr);

    // 读取视图状态并触发重绘（供 GraphicsView::viewTransformChanged 信号连接）
    void sync();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    GraphicsView* view_;
    Orientation ori_;
    static constexpr int RULER_THICKNESS = 24;  // 标尺厚度（像素）

    // 从候选 1/2/5/10/20/50/100/... 序列中选出 >= target 的最小值
    double niceStep(double target) const;
};

} // namespace SketchCAD
