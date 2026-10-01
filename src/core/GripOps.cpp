// ============================================================
// GripOps 实现 —— 夹点热操作：5 种模式 → modify::Change
// ============================================================
#include "GripOps.h"

#include <cmath>
#include <limits>
#include "Element.h"
#include "Geometry.h"

namespace SketchCAD {
namespace grip {

Mode nextMode(Mode m) {
    return static_cast<Mode>((static_cast<int>(m) + 1) % 5);
}

QString modeName(Mode m) {
    switch (m) {
    case Mode::Stretch: return QStringLiteral("拉伸");
    case Mode::Move:    return QStringLiteral("移动");
    case Mode::Rotate:  return QStringLiteral("旋转");
    case Mode::Scale:   return QStringLiteral("缩放");
    case Mode::Mirror:  return QStringLiteral("镜像");
    }
    return QStringLiteral("夹点");
}

double rotationDeg(const Grips& g, const Vec3& cursor) {
    // 旋转角 = 光标方位角 − 参考光标方位角（绕 base）
    const Vec3 d0 = g.refCursor - g.base;
    const Vec3 d1 = cursor - g.base;
    if (d0.len() < 1e-9 || d1.len() < 1e-9) return 0.0;
    double a = (d1.angleDeg() - d0.angleDeg());
    a = std::fmod(a, 360.0);
    if (a > 180.0) a -= 360.0;
    if (a < -180.0) a += 360.0;
    return a;
}

double scaleFactor(const Grips& g, const Vec3& cursor) {
    // 缩放因子 = |cursor−base| / |refCursor−base|（参考距离退化时钳制）
    const double l0 = (g.refCursor - g.base).len();
    if (l0 < 1e-9) return 1.0;   // 无参考长度 → 恒等（由拒绝逻辑兜底负值）
    const double l1 = (cursor - g.base).len();
    return l1 / l0;
}

GripResult apply(const Grips& g, Mode m, const Vec3& cursor) {
    GripResult res;

    switch (m) {
    case Mode::Stretch: {
        // 按 (图元, 控制点) 引用精确迁移：与端点拖拽同语义，
        // 重合联动图元（即使未选中）一并跟随，保持拓扑连接。
        for (const auto& ref : g.stretchRefs) {
            const Element* src = nullptr;
            for (const auto& e : g.stretchSet)
                if (e.id == ref.first) { src = &e; break; }
            if (!src) continue;
            Element e = *src;
            if (!geo::setElementHandle(e, ref.second, cursor)) continue;
            modify::Change c;
            c.id = e.id;
            c.replacement = e;
            c.note = QStringLiteral("夹点拉伸");
            res.changes.push_back(std::move(c));
        }
        break;
    }
    case Mode::Move: {
        const Vec3 delta = cursor - g.refCursor;
        res.changes = modify::makeMoveCopy(g.moveSet, delta, /*copy=*/false);
        break;
    }
    case Mode::Rotate: {
        const double ang = rotationDeg(g, cursor);
        res.changes = modify::makeRotate(g.moveSet, g.base, ang);
        break;
    }
    case Mode::Scale: {
        const double f = scaleFactor(g, cursor);
        if (!(f > 0.0) || !std::isfinite(f)) {
            res.reject = QStringLiteral("缩放比例必须为正数（当前 %1）")
                             .arg(std::isfinite(f) ? QString::number(f, 'f', 3)
                                                   : QStringLiteral("∞"));
            break;
        }
        res.changes = modify::makeScale(g.moveSet, g.base, f);
        break;
    }
    case Mode::Mirror: {
        // 镜像轴 = base → cursor；两点重合则轴退化，拒绝。
        if ((cursor - g.base).len() < 1e-9) {
            res.reject = QStringLiteral("镜像轴两点重合，无法确定镜像轴方向");
            break;
        }
        // 语义与「修改 > 镜像」一致：保留源图元、新增镜像副本
        res.changes = modify::makeMirror(g.moveSet, /*isPointMirror=*/false,
                                         g.base, cursor);
        break;
    }
    }
    return res;
}

} // namespace grip
} // namespace SketchCAD
