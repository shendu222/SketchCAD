#include "RulerWidget.h"
#include "GraphicsView.h"
#include <QPainter>
#include <QPaintEvent>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

static const QColor RULER_BG(238, 240, 243);
static const QColor RULER_BORDER(200, 204, 210);
static const QColor RULER_TICK(120, 128, 140);
static const QColor RULER_TEXT(80, 86, 96);
static const QColor RULER_ORIGIN(200, 60, 60);

RulerWidget::RulerWidget(GraphicsView* view, Orientation ori, QWidget* parent)
    : QWidget(parent), view_(view), ori_(ori) {
    setAutoFillBackground(true);
    // 标尺固定厚度，另一方向随布局拉伸
    if (ori_ == Orientation::Horizontal)
        setFixedHeight(RULER_THICKNESS);
    else
        setFixedWidth(RULER_THICKNESS);
}

QSize RulerWidget::sizeHint() const {
    return ori_ == Orientation::Horizontal
        ? QSize(200, RULER_THICKNESS)
        : QSize(RULER_THICKNESS, 200);
}

QSize RulerWidget::minimumSizeHint() const {
    return sizeHint();
}

void RulerWidget::sync() {
    update();
}

// 选择漂亮的刻度间距：1, 2, 5, 10, 20, 50, 100, 200, 500, ...
double RulerWidget::niceStep(double target) const {
    if (target <= 0) return 1.0;
    const double mag = std::pow(10.0, std::floor(std::log10(target)));
    const double norm = target / mag;  // 归一化到 [1, 10)
    double step;
    if (norm <= 1.0)      step = 1.0;
    else if (norm <= 2.0) step = 2.0;
    else if (norm <= 5.0) step = 5.0;
    else                  step = 10.0;
    return step * mag;
}

void RulerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    const double zoom = view_->zoom();
    const double panX = view_->panX();
    const double panY = view_->panY();

    // 背景
    p.fillRect(r, RULER_BG);

    // 计算可见世界坐标范围
    // 屏幕坐标 → 世界坐标：world_x = (screen_x - panX) / zoom
    //                       world_y = (panY - screen_y) / zoom   (Y 翻转)

    // 刻度间距：让主刻度在屏幕上至少间隔 60px
    const double targetStep = 60.0 / zoom;  // 60 像素对应的世界距离
    const double majorStep = niceStep(targetStep);
    const int minorDivs = (majorStep <= 1.0) ? 2 : ((int(majorStep) % 5 == 0) ? 5 : 2);
    const double minorStep = majorStep / minorDivs;

    QFont font("Arial", 8);
    p.setFont(font);
    QPen tickPen(RULER_TICK);
    tickPen.setWidthF(0.8);
    QPen minorPen(RULER_TICK);
    minorPen.setWidthF(0.5);

    if (ori_ == Orientation::Horizontal) {
        // ---- 水平标尺 ----
        const double worldLeft  = (0 - panX) / zoom;
        const double worldRight = (r.width() - panX) / zoom;

        // 底部分隔线
        p.setPen(QPen(RULER_BORDER, 1));
        p.drawLine(r.bottomLeft(), r.bottomRight());

        // 次刻度
        p.setPen(minorPen);
        double firstMinor = std::floor(worldLeft / minorStep) * minorStep;
        for (double w = firstMinor; w <= worldRight + minorStep; w += minorStep) {
            int sx = int(w * zoom + panX);
            if (sx < 0 || sx > r.width()) continue;
            p.drawLine(sx, r.height(), sx, r.height() - 4);
        }

        // 主刻度 + 数字
        p.setPen(tickPen);
        double firstMajor = std::floor(worldLeft / majorStep) * majorStep;
        for (double w = firstMajor; w <= worldRight + majorStep; w += majorStep) {
            int sx = int(w * zoom + panX);
            if (sx < 0 || sx > r.width()) continue;

            // 主刻度线
            p.drawLine(sx, r.height(), sx, r.height() - 9);

            // 数字标签（跳过 0 的文字以避免与原点标记重叠，但画原点指示线）
            if (std::abs(w) < majorStep * 0.01) {
                // 原点：用红色短线标记
                p.setPen(QPen(RULER_ORIGIN, 1.5));
                p.drawLine(sx, r.height(), sx, 0);
                p.setPen(tickPen);
            } else {
                QString label;
                double absW = std::abs(w);
                if (absW >= 1000)
                    label = QString::number(w / 1000.0, 'f', absW >= 10000 ? 0 : 1) + "k";
                else if (absW >= 100)
                    label = QString::number(w, 'f', 0);
                else if (absW >= 10)
                    label = QString::number(w, 'f', 0);
                else
                    label = QString::number(w, 'f', 1);

                p.drawText(QRect(sx - 30, 0, 60, r.height() - 10),
                           Qt::AlignCenter, label);
            }
        }
    } else {
        // ---- 垂直标尺 ----
        // Y 翻转：screen_y = -world_y * zoom + panY → world_y = (panY - screen_y) / zoom
        const double worldTop    = (panY - 0) / zoom;
        const double worldBottom = (panY - r.height()) / zoom;

        // 右侧分隔线
        p.setPen(QPen(RULER_BORDER, 1));
        p.drawLine(r.topRight(), r.bottomRight());

        // 次刻度
        p.setPen(minorPen);
        double firstMinor = std::floor(worldBottom / minorStep) * minorStep;
        for (double w = firstMinor; w <= worldTop + minorStep; w += minorStep) {
            int sy = int(panY - w * zoom);
            if (sy < 0 || sy > r.height()) continue;
            p.drawLine(r.width(), sy, r.width() - 4, sy);
        }

        // 主刻度 + 数字
        p.setPen(tickPen);
        double firstMajor = std::floor(worldBottom / majorStep) * majorStep;
        for (double w = firstMajor; w <= worldTop + majorStep; w += majorStep) {
            int sy = int(panY - w * zoom);
            if (sy < 0 || sy > r.height()) continue;

            // 主刻度线
            p.drawLine(r.width(), sy, r.width() - 9, sy);

            // 数字标签
            if (std::abs(w) < majorStep * 0.01) {
                // 原点：用红色短线标记
                p.setPen(QPen(RULER_ORIGIN, 1.5));
                p.drawLine(r.width(), sy, 0, sy);
                p.setPen(tickPen);
            } else {
                QString label;
                double absW = std::abs(w);
                if (absW >= 1000)
                    label = QString::number(w / 1000.0, 'f', absW >= 10000 ? 0 : 1) + "k";
                else if (absW >= 100)
                    label = QString::number(w, 'f', 0);
                else if (absW >= 10)
                    label = QString::number(w, 'f', 0);
                else
                    label = QString::number(w, 'f', 1);

                // 垂直标尺：文字旋转 -90° 使其纵向可读
                p.save();
                p.translate(r.width() - 10, sy);
                p.rotate(-90);
                p.drawText(QRect(-30, -8, 60, 16), Qt::AlignCenter, label);
                p.restore();
            }
        }
    }
}

} // namespace SketchCAD
