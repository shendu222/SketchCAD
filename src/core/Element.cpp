#include "Element.h"
#include <cmath>

namespace SketchCAD {

QString Element::sizeDesc() const {
    switch (type) {
        case ElementType::Line:
            return pts.size() >= 2
                ? QString("长度 %1 mm").arg(pts[0].distTo(pts[1]), 0, 'f', 2)
                : QString();
        case ElementType::Circle:
        case ElementType::Arc:
            return QString("半径 %1 mm").arg(radius, 0, 'f', 2);
        case ElementType::Ellipse:
            return QString("长轴 %1 / 短轴 %2 mm").arg(radius, 0, 'f', 2).arg(radiusY, 0, 'f', 2);
        case ElementType::Rectangle:
            if (pts.size() >= 2) {
                double w = std::fabs(pts[1].x - pts[0].x);
                double h = std::fabs(pts[1].y - pts[0].y);
                return QString("%1 × %2 mm").arg(w, 0, 'f', 2).arg(h, 0, 'f', 2);
            }
            return QString();
        case ElementType::Polygon:
        case ElementType::Spline:
            return QString("顶点数 %1").arg(pts.size());
        case ElementType::Polyline: {
            // 折线：节点数 + 弧段数 + 总长
            const int nNodes = (int)pts.size();
            const int nSegs  = nNodes - (closed ? 0 : 1);
            int nArcs = 0;
            for (bool a : arcFlags) if (a) ++nArcs;
            // 总长：直线段取端点距离；弧段用 chord + 半径粗估
            double total = 0;
            for (int i = 0; i < (closed ? nNodes : nNodes - 1); ++i) {
                const Vec3& a = pts[i];
                const Vec3& b = pts[(i + 1) % nNodes];
                if (i < (int)arcFlags.size() && arcFlags[i]) {
                    // 弦长 + 弧高近似，简化用 chord*1.05
                    total += a.distTo(b) * 1.05;
                } else {
                    total += a.distTo(b);
                }
            }
            return QString("节点 %1 / 段 %2 / 弧段 %3 / 总长 %4 mm")
                   .arg(nNodes).arg(nSegs).arg(nArcs).arg(total, 0, 'f', 2);
        }
        case ElementType::Constraint:
            return constraintTypeName(ctype);
        case ElementType::Dimension:
            return text.isEmpty() ? QStringLiteral("标注") : text;
        case ElementType::Text:
            return QString("文本 高度 %1 mm").arg(radius, 0, 'f', 2);
        case ElementType::Hatch:
            return QString("剖面区域 角度 %1° / 间距 %2 mm / 内孔 %3")
                   .arg(rotation, 0, 'f', 1).arg(radius, 0, 'f', 2).arg(holes.size());
    }
    return QString();
}

} // namespace SketchCAD
