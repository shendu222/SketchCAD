#include "core/PolylineOps.h"
#include "core/Geometry.h"
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace polyops {

static bool isPolylineTarget(SketchModel& m, int id) {
    const Element* e = m.element(id);
    return e && e->type == ElementType::Polyline;
}

bool setClosed(SketchModel& m, int id, bool close) {
    if (!isPolylineTarget(m, id)) {
        m.log(QStringLiteral("目标不是折线"), "warn");
        return false;
    }
    const Element* pe = m.element(id);
    if (pe->closed == close) {
        // 已是目标状态，视为成功（幂等）
        return true;
    }
    m.updateElement(id, [close](Element& out) {
        const int n = (int)out.pts.size();
        if (close) {
            out.closed = true;
            // 闭合段（pts[n-1] -> pts[0]）需要一个 arcFlag；若该标志缺失则补 false（直线段）
            if ((int)out.arcFlags.size() < n) out.arcFlags.push_back(false);
        } else {
            out.closed = false;
            // 打开：去掉闭合段对应的 arcFlag（开放折线 arcFlags 长度应为 n-1）
            const int want = std::max(0, n - 1);
            if ((int)out.arcFlags.size() > want) out.arcFlags.resize(want);
        }
    });
    m.log(close ? QStringLiteral("折线已闭合") : QStringLiteral("折线已打开"));
    return true;
}

bool reverse(SketchModel& m, int id) {
    if (!isPolylineTarget(m, id)) {
        m.log(QStringLiteral("目标不是折线"), "warn");
        return false;
    }
    m.updateElement(id, [](Element& out) {
        std::reverse(out.pts.begin(), out.pts.end());
        std::reverse(out.arcFlags.begin(), out.arcFlags.end());
    });
    m.log(QStringLiteral("折线已反转"));
    return true;
}

bool setWidth(SketchModel& m, int id, double w) {
    if (!isPolylineTarget(m, id)) {
        m.log(QStringLiteral("目标不是折线"), "warn");
        return false;
    }
    if (w < 0) w = 0;
    m.updateElement(id, [w](Element& out) {
        out.lineWidthAuto = false;   // 覆盖线型自动派生，使用手动线宽
        out.lineWidth = w;
    });
    m.log(QString("折线线宽已设为 %1").arg(w, 0, 'f', 2));
    return true;
}

bool fit(SketchModel& m, int id) {
    if (!isPolylineTarget(m, id)) {
        m.log(QStringLiteral("目标不是折线"), "warn");
        return false;
    }
    const Element* pe = m.element(id);
    if ((int)pe->pts.size() < 2) {
        m.log(QStringLiteral("折线顶点不足，无法拟合"), "warn");
        return false;
    }
    m.updateElement(id, [](Element& out) { out.splineSubType = 1; });   // 1 = Fit
    m.log(QStringLiteral("折线已拟合（平滑通过所有顶点）"));
    return true;
}

bool spline(SketchModel& m, int id) {
    if (!isPolylineTarget(m, id)) {
        m.log(QStringLiteral("目标不是折线"), "warn");
        return false;
    }
    const Element* pe = m.element(id);
    if ((int)pe->pts.size() < 2) {
        m.log(QStringLiteral("折线顶点不足，无法样条化"), "warn");
        return false;
    }
    m.updateElement(id, [](Element& out) { out.splineSubType = 2; });   // 2 = Spline
    m.log(QStringLiteral("折线已样条化（顶点为控制点）"));
    return true;
}

// 取直线/圆弧/开放折线的两个端点（Arc 用圆心+半径+角度反算）
static bool endpointsOf(const Element& s, Vec3& a, Vec3& b, std::vector<Vec3>& ordered) {
    ordered.clear();
    if (s.type == ElementType::Line && s.pts.size() >= 2) {
        a = s.pts[0]; b = s.pts[1];
        ordered = {s.pts[0], s.pts[1]};
        return true;
    }
    if (s.type == ElementType::Polyline && !s.closed && s.pts.size() >= 2) {
        a = s.pts.front(); b = s.pts.back();
        ordered = s.pts;
        return true;
    }
    if (s.type == ElementType::Arc && s.radius > 1e-9) {
        const Vec3 c = s.pts[0];
        const double R = s.radius;
        double a0 = math::deg2rad(s.startAngle);
        double a1 = math::deg2rad(s.endAngle);
        a = Vec3(c.x + R * std::cos(a0), c.y + R * std::sin(a0), 0);
        b = Vec3(c.x + R * std::cos(a1), c.y + R * std::sin(a1), 0);
        // 沿扫掠方向采样圆弧（作为直线段序列接入）
        double sweep = s.endAngle - s.startAngle;
        while (sweep > 360.0) sweep -= 360.0;
        while (sweep < -360.0) sweep += 360.0;
        const int steps = 24;
        for (int k = 0; k <= steps; ++k) {
            const double ang = math::deg2rad(s.startAngle + sweep * (double)k / steps);
            ordered.push_back(Vec3(c.x + R * std::cos(ang), c.y + R * std::sin(ang), 0));
        }
        return true;
    }
    return false;
}

bool join(SketchModel& m, int targetId, int sourceId, double tol) {
    if (!isPolylineTarget(m, targetId)) {
        m.log(QStringLiteral("合并目标必须是折线"), "warn");
        return false;
    }
    if (targetId == sourceId) return false;
    const Element* pt = m.element(targetId);
    const Element* ps = m.element(sourceId);
    if (!pt || !ps) return false;
    if (pt->closed) {
        m.log(QStringLiteral("目标折线已闭合，请先打开再合并"), "warn");
        return false;
    }
    if (ps->type != ElementType::Line &&
        ps->type != ElementType::Arc &&
        ps->type != ElementType::Polyline) {
        m.log(QStringLiteral("合并来源必须是直线/圆弧/折线"), "warn");
        return false;
    }
    if (ps->type == ElementType::Polyline && ps->closed) {
        m.log(QStringLiteral("来源折线已闭合，无法合并"), "warn");
        return false;
    }

    Vec3 tStart = pt->pts.front();
    Vec3 tEnd   = pt->pts.back();

    Vec3 sA, sB;
    std::vector<Vec3> sPts;
    if (!endpointsOf(*ps, sA, sB, sPts)) {
        m.log(QStringLiteral("无法取得来源端点"), "warn");
        return false;
    }

    bool attachEnd = false;
    std::vector<Vec3> tail;   // 待接上的中间+另一端有序点（不含重合共享点）

    auto buildTailFromA = [&](const std::vector<Vec3>& seq) {
        // seq 起点为共享点 A，输出 seq 去掉首点后的剩余
        return std::vector<Vec3>(seq.begin() + 1, seq.end());
    };
    auto buildTailFromB = [&](const std::vector<Vec3>& seq) {
        // seq 末点为共享点 B，输出反向后去掉首点（即原末点）
        std::vector<Vec3> rev(seq.rbegin(), seq.rend());
        return std::vector<Vec3>(rev.begin() + 1, rev.end());
    };

    if (tEnd.distTo(sA) <= tol) {
        attachEnd = true;
        tail = buildTailFromA(sPts);
    } else if (tEnd.distTo(sB) <= tol) {
        attachEnd = true;
        tail = buildTailFromB(sPts);
    } else if (tStart.distTo(sA) <= tol) {
        tail = buildTailFromA(sPts);   // 共享点 sA 在 tStart，保留来源其余点（B..A 间）
    } else if (tStart.distTo(sB) <= tol) {
        tail = buildTailFromB(sPts);   // 共享点 sB 在 tStart，倒序后保留 A..(B前)
    } else {
        m.log(QStringLiteral("两端点不重合，无法合并"), "warn");
        return false;
    }

    m.updateElement(targetId, [attachEnd, &tail](Element& out) {
        if (attachEnd) {
            for (const auto& v : tail) out.pts.push_back(v);
            for (size_t k = 0; k < tail.size(); ++k) out.arcFlags.push_back(false);
        } else {  // attachStart：在首端前插
            std::vector<Vec3> np = tail;
            np.insert(np.end(), out.pts.begin(), out.pts.end());
            out.pts = std::move(np);
            std::vector<bool> nf;
            for (size_t k = 0; k < tail.size(); ++k) nf.push_back(false);
            nf.insert(nf.end(), out.arcFlags.begin(), out.arcFlags.end());
            out.arcFlags = std::move(nf);
        }
    });
    m.removeElement(sourceId);
    m.log(QStringLiteral("已合并到折线"));
    return true;
}

} // namespace polyops
} // namespace SketchCAD
