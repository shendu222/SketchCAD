// 剖面填充相关测试的公共辅助
// ----------------------------------------------------------------------------
// 原先 check / idsToStr / sameIdSet / makeLine / makeRect / makeCircle / makeArc /
// makeOpenPolyline / makeClosedPolyline / findHatch / distToPolygon 等在各测试文件里
// 各写一份（逐字重复），改动一处契约就要同步多份、极易漂移。统一收敛到本头文件，
// 用 inline（C++17 inline 变量 / 函数）保证多 TU 包含时只有一份实体。
//
// 用法：测试文件 `#include "section_test_helpers.h"`，并删除本地同名定义。
// （本头文件已 `using namespace SketchCAD;`，与既有测试文件的写法一致。）
#pragma once

#include "core/Section.h"
#include "core/SketchModel.h"
#include "core/Modify.h"
#include "core/Geometry.h"
#include "core/Element.h"
#include "core/Vec.h"

#include <QString>
#include <QStringList>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <vector>

using namespace SketchCAD;

// ---- 断言 + 计数 -----------------------------------------------------------
inline int g_pass = 0;
inline int g_fail = 0;

inline void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); ++g_pass; }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}

// ---- 比较 / 格式化 ---------------------------------------------------------
inline QString idsToStr(const std::vector<int>& v) {
    QStringList l; for (int i : v) l << QString::number(i); return l.join(QStringLiteral(","));
}

inline bool sameIdSet(std::vector<int> a, std::vector<int> b) {
    std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end()); return a == b;
}

// ---- 图元构造 --------------------------------------------------------------
inline Element makeLine(const Vec3& a, const Vec3& b, int id = 0) {
    Element e; e.type = ElementType::Line; e.pts = { a, b }; e.id = id; return e;
}
inline Element makeCircle(const Vec3& c, double r, int id = 0) {
    Element e; e.type = ElementType::Circle; e.pts = { c }; e.radius = r; e.id = id; return e;
}
inline Element makeArc(const Vec3& c, double r, double a0, double a1, int id = 0) {
    Element e; e.type = ElementType::Arc; e.pts = { c }; e.radius = r;
    e.startAngle = a0; e.endAngle = a1; e.id = id; return e;
}
inline Element makeRect(double x0, double y0, double x1, double y1, int id = 0) {
    Element e; e.type = ElementType::Rectangle;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y1, 0) }; e.id = id; return e;
}
inline Element makeOpenPolyline(const std::vector<Vec3>& pts, int id = 0) {
    Element e; e.type = ElementType::Polyline; e.closed = false; e.pts = pts; e.id = id; return e;
}
inline Element makeClosedPolyline(const std::vector<Vec3>& pts, int id = 0) {
    Element e; e.type = ElementType::Polyline; e.closed = true; e.pts = pts; e.id = id; return e;
}
inline Element makePolygon(const std::vector<Vec3>& pts, int id = 0) {
    Element e; e.type = ElementType::Polygon; e.closed = true; e.pts = pts; e.id = id; return e;
}

// ---- 数值 / 几何小工具 -----------------------------------------------------
inline bool near2(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

// 轴对齐矩形四个角点（逆时针）
inline std::vector<Vec3> square(double x0, double y0, double x1, double y1) {
    return { Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0) };
}

// ---- 几何 / 断言辅助 -------------------------------------------------------
// 落盘后从模型里找出那个 Hatch（不假设它落在哪个位置）
inline const Element* findHatch(const SketchModel& m) {
    for (const auto& e : m.elements())
        if (e.type == ElementType::Hatch) return &e;
    return nullptr;
}

// 点到（隐式闭合）多边形边界的最近距离
inline double distToPolygon(const Vec3& p, const std::vector<Vec3>& poly) {
    double best = 1e100;
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i)
        best = std::min(best, geo::distPointSegment(p, poly[i], poly[(i + 1) % n]));
    return best;
}

// 严格验证：剖面线段完全落在闭合边界内（不渗漏 / 不溢出）——
// 端点落在边界上(距离≈0)或边界内；中点必须在边界内。
inline bool segWithinLoop(const geo::Seg& s, const std::vector<Vec3>& poly, QString* why = nullptr) {
    const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
    if (!geo::pointInPolygon(mid, poly)) { if (why) *why = "midpoint outside"; return false; }
    for (const Vec3& p : { s.a, s.b }) {
        const double d = distToPolygon(p, poly);
        if (d > 1e-6 && !geo::pointInPolygon(p, poly)) {
            if (why) *why = QString("endpoint off-boundary: %1").arg(d);
            return false;
        }
    }
    return true;
}
