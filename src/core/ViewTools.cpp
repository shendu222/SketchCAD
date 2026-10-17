#include "ViewTools.h"
#include "Geometry.h"     // geo::rotateAroundCentroid（既有 Transform 原语，转角投影复用）
#include <cmath>

namespace SketchCAD {
namespace {

constexpr double K_PI = 3.14159265358979323846;

Element mkLine(const Vec3& a, const Vec3& b, double w, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Line;
    e.pts = {a, b};
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = w; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}
Element mkCenterLine(const Vec3& a, const Vec3& b, const QString& layer, const QString& name) {
    Element e = mkLine(a, b, 0.5, layer, name);
    e.lineStyle = LineStyle::DashDot;   // GB 中心线（细点画线）
    return e;
}
Element mkCircle(const Vec3& c, double r, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Circle;
    e.pts = {c};
    e.radius = r;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 0.7; e.lineWidthAuto = false;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}
Element mkText(const Vec3& pos, const QString& t, double h, const QString& layer, const QString& name) {
    Element e;
    e.type = ElementType::Text;
    e.pts = {pos};
    e.text = t;
    e.radius = h;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidth = 1.0; e.lineWidthAuto = true;
    e.color = QColor(26, 29, 40);
    e.name = name;
    return e;
}
Vec3 dirOf(double angleDeg) {
    const double a = angleDeg * K_PI / 180.0;
    return Vec3(std::cos(a), std::sin(a), 0.0);
}
} // namespace

// ------------------------------------------------------------
// 向视符号：箭杆 + V 形箭头 + 字母
// ------------------------------------------------------------
bool buildDirectionSymbol(const DirectionSymbolSpec& s, std::vector<Element>& out) {
    out.clear();
    if (!s.hasDirection || s.arrowLen <= 0.0) return false;   // 缺方向 / 箭长非正 → false
    const Vec3 dir = dirOf(s.angleDeg);
    const Vec3 head = Vec3(s.anchor.x + dir.x * s.arrowLen, s.anchor.y + dir.y * s.arrowLen, 0.0);
    // 箭杆（细实线）
    out.push_back(mkLine(s.anchor, head, 0.7, s.layer, QStringLiteral("向视箭头")));
    // V 形箭头（半张角 15°，自箭头前端反向张开）
    const double wing = s.arrowLen * 0.30;
    const double r15 = 15.0 * K_PI / 180.0;
    const double bx = -dir.x, by = -dir.y;
    auto wingTip = [&](double sign) {
        const double cs = std::cos(sign * r15), sn = std::sin(sign * r15);
        return Vec3(head.x + (bx * cs - by * sn) * wing, head.y + (bx * sn + by * cs) * wing, 0.0);
    };
    out.push_back(mkLine(head, wingTip(+1.0), 0.7, s.layer, QStringLiteral("向视箭头")));
    out.push_back(mkLine(head, wingTip(-1.0), 0.7, s.layer, QStringLiteral("向视箭头")));
    // 字母标识：箭杆中点、沿法向偏移
    const Vec3 mid = Vec3(s.anchor.x + dir.x * s.arrowLen * 0.5, s.anchor.y + dir.y * s.arrowLen * 0.5, 0.0);
    const Vec3 perp(-dir.y, dir.x, 0.0);
    out.push_back(mkText(Vec3(mid.x + perp.x * s.textHeight * 1.2,
                              mid.y + perp.y * s.textHeight * 1.2, 0.0),
                         s.letter, s.textHeight, s.layer, QStringLiteral("向视字母")));
    return true;
}

// ------------------------------------------------------------
// 孔轴投影：中心线 + 两端投影圆 + 两侧轮廓母线
// ------------------------------------------------------------
bool buildHoleProjection(const HoleProjectionSpec& s, std::vector<Element>& out) {
    out.clear();
    if (!s.hasDirection || s.diameter <= 0.0 || s.depth <= 0.0) return false;
    const Vec3 dir = dirOf(s.angleDeg);
    const double r = s.diameter * 0.5;
    const Vec3 far = Vec3(s.anchor.x + dir.x * s.depth, s.anchor.y + dir.y * s.depth, 0.0);
    // 中心线（点画线），两端各超出 2mm
    const Vec3 p0 = Vec3(s.anchor.x - dir.x * 2.0, s.anchor.y - dir.y * 2.0, 0.0);
    const Vec3 p1 = Vec3(far.x + dir.x * 2.0, far.y + dir.y * 2.0, 0.0);
    out.push_back(mkCenterLine(p0, p1, s.layer, QStringLiteral("孔轴中心线")));
    // 两端投影圆（近端 + 远端）
    out.push_back(mkCircle(s.anchor, r, s.layer, QStringLiteral("投影圆")));
    out.push_back(mkCircle(far, r, s.layer, QStringLiteral("投影圆")));
    // 两侧轮廓母线（连接两圆轮廓，±法向）
    const Vec3 perp(-dir.y, dir.x, 0.0);
    out.push_back(mkLine(Vec3(s.anchor.x + perp.x * r, s.anchor.y + perp.y * r, 0.0),
                         Vec3(far.x + perp.x * r, far.y + perp.y * r, 0.0),
                         0.7, s.layer, QStringLiteral("投影轮廓")));
    out.push_back(mkLine(Vec3(s.anchor.x - perp.x * r, s.anchor.y - perp.y * r, 0.0),
                         Vec3(far.x - perp.x * r, far.y - perp.y * r, 0.0),
                         0.7, s.layer, QStringLiteral("投影轮廓")));
    return true;
}

// ------------------------------------------------------------
// 转角投影：折线 p0→p1→p2（第二段方向 = 第一段方向经转角旋转）
// ------------------------------------------------------------
bool buildRotationProjection(const RotationProjectionSpec& s, std::vector<Element>& out) {
    out.clear();
    if (!s.hasAngle || s.length1 <= 0.0 || s.length2 <= 0.0) return false;
    const Vec3 d1 = dirOf(s.startAngleDeg);
    // 复用既有 Transform 原语旋转方向（FORBID：不自造投影矩阵）
    const auto rot = geo::rotateAroundCentroid({ d1 }, Vec3(0, 0, 0), s.turnAngleDeg);
    const Vec3 d2 = rot.empty() ? d1 : rot[0];   // 单位向量（旋转保长）
    const Vec3 p0 = s.anchor;
    const Vec3 p1 = Vec3(p0.x + d1.x * s.length1, p0.y + d1.y * s.length1, 0.0);
    const Vec3 p2 = Vec3(p1.x + d2.x * s.length2, p1.y + d2.y * s.length2, 0.0);
    Element pl;
    pl.type = ElementType::Polyline;
    pl.pts = {p0, p1, p2};
    pl.closed = false;
    pl.layer = s.layer;
    pl.name = QStringLiteral("转角投影");
    pl.lineStyle = LineStyle::Solid;
    pl.lineWidthAuto = true;
    pl.color = QColor(26, 29, 40);
    out.push_back(pl);
    return true;
}

} // namespace SketchCAD
