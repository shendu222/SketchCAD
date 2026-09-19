#include "FlangeHubViews.h"
#include "ElementFactory.h"
#include "SectionDrawing.h"
#include "SurfaceRoughness.h"
#include "SectionCutSymbol.h"
#include "Geometry.h"        // geo::elementBounds
#include "SketchModel.h"
#include <QColor>
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace {

// ----------------------------- 图层与线型约定 -----------------------------
// 图层前缀「轮毂法兰」，与其它模板（法兰/箱体/轴承座）互不冲突，便于 BOM 树分组控制。
const QString LY_OUTLINE = QStringLiteral("轮毂法兰-轮廓");   // 粗实线
const QString LY_HIDDEN  = QStringLiteral("轮毂法兰-虚线");   // 细虚线（预留）
const QString LY_CENTER  = QStringLiteral("轮毂法兰-中心线"); // 点划线
const QString LY_SECT    = QStringLiteral("轮毂法兰-剖面");   // 剖面线（Builder 内部用）
const QString LY_DIM     = QStringLiteral("轮毂法兰-标注");
const QString LY_TEXT    = QStringLiteral("轮毂法兰-文字");

const QColor C_OUTLINE(26, 29, 40);
const QColor C_CENTER (170, 60, 60);

constexpr double K_PI = 3.14159265358979323846;

// ----------------------------- 图元构造辅助 -----------------------------
Element mkLeader(const Vec3& tip, const Vec3& knee, const Vec3& anchor,
                 const QString& text, const SketchModel* m, double dimTextH) {
    Element e = ElementFactory::createDimension(DimensionType::Leader, m);
    e.dtype = DimensionType::Leader;
    e.pts = {tip, knee, anchor};
    e.text = text;
    e.layer = LY_DIM;
    e.name = QStringLiteral("引线标注");
    e.dimTextHeight = dimTextH;
    return e;
}

// ----------------------------- 数值格式化 -----------------------------
QString n2(double v) {
    const double r = std::round(v);
    if (std::fabs(v - r) < 1e-9) return QString::number((long long)r);
    return QString::number(v, 'f', 1);
}
QString dimD(double v) { return QStringLiteral("⌀") + n2(v); }

const double kStdScales[] = {50.0, 20.0, 10.0, 5.0, 2.0, 1.0,
                             1.0 / 2.0, 1.0 / 2.5, 1.0 / 5.0, 1.0 / 10.0,
                             1.0 / 20.0, 1.0 / 50.0};
QString scaleLabel(double s) {
    if (std::fabs(s - 1.0) < 1e-9) return QStringLiteral("1:1");
    if (s > 1.0) return QStringLiteral("%1:1").arg(n2(s));
    return QStringLiteral("1:%1").arg(n2(1.0 / s));
}

// ---- 纸面常量（不随比例缩放，mm）----
constexpr double kLeftBand  = 38.0;  // 主视图左侧：4 道竖直直径标注
constexpr double kRightPad  = 16.0;  // 主视图右侧：引线标注
constexpr double kTopPad    = 12.0;  // 上方：剖视图名 A—A + 引线
constexpr double kBotBand   = 24.0;  // 下方：3 道水平尺寸
constexpr double kCenterExt = 6.0;   // 中心线伸出圆外的长度（图纸量）

// 主视图剖切面处的上半材料边界多边形（模型坐标：x=轴向 0..totalL，y=半径；闭合）。
// 法兰根部内凹圆角用 8 段折线逼近（仅用于剖面线裁剪；可见轮廓另用精确圆弧绘制）。
std::vector<Vec3> upperBoundary(const FlangeHubParams& p) {
    const double rF = p.flangeR(), rH = p.hubR(), rB = p.boreR(), rC = p.cboreR();
    const double xH = p.hubFlangeX(), cd = p.cboreDepth, chf = p.chamfer, rFilt = p.filletR;
    std::vector<Vec3> pts;
    pts.push_back(Vec3(cd, rB, 0));              // 沉孔底与通孔交界
    pts.push_back(Vec3(p.totalL, rB, 0));        // 通孔面
    pts.push_back(Vec3(p.totalL, rF, 0));        // 法兰右端面
    pts.push_back(Vec3(xH, rF, 0));              // 法兰外圆
    if (rFilt > 1e-6) {
        // 内凹圆角：圆心 (xH−rFilt, rH+rFilt)，从 T2=(xH, rH+rFilt) 到 T1=(xH−rFilt, rH)，鼓向角点 (xH, rH)
        const double ox = xH - rFilt, oy = rH + rFilt;
        pts.push_back(Vec3(xH, rH + rFilt, 0));
        const int n = 8;
        for (int i = 1; i < n; ++i) {
            const double a = -K_PI * 0.5 * i / n;      // 0° → −90°
            pts.push_back(Vec3(ox + rFilt * std::cos(a), oy + rFilt * std::sin(a), 0));
        }
        pts.push_back(Vec3(xH - rFilt, rH, 0));
    } else {
        pts.push_back(Vec3(xH, rH, 0));
    }
    pts.push_back(Vec3(chf, rH, 0));             // 轮毂外圆
    pts.push_back(Vec3(0.0, rH - chf, 0));       // 左端倒角
    pts.push_back(Vec3(0.0, rC, 0));             // 左端面
    pts.push_back(Vec3(cd, rC, 0));              // 沉孔底
    return pts;                                   // 闭合回 (cd, rB)
}

// 螺栓孔在剖切面中的矩形（上下对称）。放不进法兰环带时返回空（并置 warn）。
struct HoleRects {
    bool warn = false;
    std::vector<std::vector<Vec3>> rects;   // 各 4 点，模型坐标
};
HoleRects boltHoleRects(const FlangeHubParams& p) {
    HoleRects hr;
    if (p.boltCount <= 0) return hr;
    const double xH = p.hubFlangeX();
    const double y0 = p.boltBCD * 0.5 - p.boltD * 0.5;
    const double y1 = p.boltBCD * 0.5 + p.boltD * 0.5;
    if (y0 <= p.boreR() + 0.5 || y1 >= p.flangeR() - 0.5) { hr.warn = true; return hr; }
    const std::vector<Vec3> up = {
        Vec3(xH, y0, 0), Vec3(p.totalL, y0, 0),
        Vec3(p.totalL, y1, 0), Vec3(xH, y1, 0)};
    const std::vector<Vec3> lo = {
        Vec3(xH, -y1, 0), Vec3(p.totalL, -y1, 0),
        Vec3(p.totalL, -y0, 0), Vec3(xH, -y0, 0)};
    hr.rects.push_back(up);
    hr.rects.push_back(lo);
    return hr;
}

// ---- 主视图（全剖 A—A）：轴线上 y=axisY，零件左端 x=fx0（图纸坐标）----
void buildFrontSection(std::vector<Element>& out, const FlangeHubParams& p, double s,
                       double fx0, double axisY, const SketchModel* model,
                       FlangeHubViewsResult& R) {
    SectionViewBuilder sb(QStringLiteral("轮毂法兰"));
    SectionViewStyle st;
    st.hatchAngleDeg  = p.hatchAngleDeg;
    st.hatchSpacingMm = p.hatchSpacingMm;
    st.textHeight     = p.dimTextHeight;
    sb.setStyle(st);

    auto X = [&](double mx) { return fx0 + mx * s; };
    auto Y = [&](double my) { return axisY + my * s; };
    const double rF = p.flangeR(), rH = p.hubR(), rB = p.boreR(), rC = p.cboreR();
    const double xH = p.hubFlangeX(), cd = p.cboreDepth, chf = p.chamfer, rFilt = p.filletR;

    // ---- 剖面区域：上/下两半材料边界，螺栓孔矩形作为孔洞剔除（剖面线自动避让）----
    // 注意：upperBoundary / boltHoleRects 返回的是模型坐标，必须经 X()/Y() 变换到图纸坐标，
    // 否则剖面线会被画到原点附近（与可见轮廓错位，且包围盒越出图框）。
    const HoleRects hr = boltHoleRects(p);
    const std::vector<Vec3> upBm = upperBoundary(p);
    std::vector<Vec3> upB, loB;
    upB.reserve(upBm.size()); loB.reserve(upBm.size());
    for (const Vec3& v : upBm) {
        upB.push_back(Vec3(X(v.x), Y(v.y), 0));
        loB.push_back(Vec3(X(v.x), Y(-v.y), 0));
    }
    std::vector<std::vector<Vec3>> holeD;
    for (const auto& rect : hr.rects) {
        std::vector<Vec3> rd;
        for (const Vec3& v : rect) rd.push_back(Vec3(X(v.x), Y(v.y), 0));
        holeD.push_back(std::move(rd));
    }

    if (holeD.empty()) {
        sb.addCutRegion(upB, {}, false);
        sb.addCutRegion(loB, {}, false);
    } else {
        sb.addCutRegion(upB, {holeD[0]}, false);
        sb.addCutRegion(loB, {holeD[1]}, false);
    }

    // ---- 可见轮廓（粗实线）----
    // 上半：通孔面 → 法兰右端面 → 法兰外圆 → 法兰左端面 → 内凹圆弧 → 轮毂外圆 →
    //        倒角 → 左端面 → 沉孔底 → 沉孔台阶
    sb.addVisibleLine(Vec3(X(cd), Y(rB), 0),  Vec3(X(p.totalL), Y(rB), 0));
    sb.addVisibleLine(Vec3(X(p.totalL), Y(rB), 0), Vec3(X(p.totalL), Y(rF), 0));
    sb.addVisibleLine(Vec3(X(p.totalL), Y(rF), 0), Vec3(X(xH), Y(rF), 0));
    if (rFilt > 1e-6) {
        sb.addVisibleLine(Vec3(X(xH), Y(rF), 0), Vec3(X(xH), Y(rH + rFilt), 0));
        sb.addVisibleArc(Vec3(X(xH - rFilt), Y(rH + rFilt), 0), rFilt * s, -90.0, 0.0);
        sb.addVisibleLine(Vec3(X(xH - rFilt), Y(rH), 0), Vec3(X(chf), Y(rH), 0));
    } else {
        sb.addVisibleLine(Vec3(X(xH), Y(rF), 0), Vec3(X(xH), Y(rH), 0));
        sb.addVisibleLine(Vec3(X(xH), Y(rH), 0), Vec3(X(chf), Y(rH), 0));
    }
    sb.addVisibleLine(Vec3(X(chf), Y(rH), 0), Vec3(X(0.0), Y(rH - chf), 0));
    sb.addVisibleLine(Vec3(X(0.0), Y(rH - chf), 0), Vec3(X(0.0), Y(rC), 0));
    sb.addVisibleLine(Vec3(X(0.0), Y(rC), 0),  Vec3(X(cd), Y(rC), 0));
    sb.addVisibleLine(Vec3(X(cd), Y(rC), 0),  Vec3(X(cd), Y(rB), 0));
    // 下半（镜像）
    sb.addVisibleLine(Vec3(X(cd), Y(-rB), 0),  Vec3(X(p.totalL), Y(-rB), 0));
    sb.addVisibleLine(Vec3(X(p.totalL), Y(-rB), 0), Vec3(X(p.totalL), Y(-rF), 0));
    sb.addVisibleLine(Vec3(X(p.totalL), Y(-rF), 0), Vec3(X(xH), Y(-rF), 0));
    if (rFilt > 1e-6) {
        sb.addVisibleLine(Vec3(X(xH), Y(-rF), 0), Vec3(X(xH), Y(-(rH + rFilt)), 0));
        sb.addVisibleArc(Vec3(X(xH - rFilt), Y(-(rH + rFilt)), 0), rFilt * s, 0.0, 90.0);
        sb.addVisibleLine(Vec3(X(xH - rFilt), Y(-rH), 0), Vec3(X(chf), Y(-rH), 0));
    } else {
        sb.addVisibleLine(Vec3(X(xH), Y(-rF), 0), Vec3(X(xH), Y(-rH), 0));
        sb.addVisibleLine(Vec3(X(xH), Y(-rH), 0), Vec3(X(chf), Y(-rH), 0));
    }
    sb.addVisibleLine(Vec3(X(chf), Y(-rH), 0), Vec3(X(0.0), Y(-(rH - chf)), 0));
    sb.addVisibleLine(Vec3(X(0.0), Y(-(rH - chf)), 0), Vec3(X(0.0), Y(-rC), 0));
    sb.addVisibleLine(Vec3(X(0.0), Y(-rC), 0),  Vec3(X(cd), Y(-rC), 0));
    sb.addVisibleLine(Vec3(X(cd), Y(-rC), 0),  Vec3(X(cd), Y(-rB), 0));

    // ---- 螺栓孔剖面轮廓（孔在剖切面处的矩形边界）----
    for (const auto& rect : hr.rects) {
        for (size_t i = 0; i < rect.size(); ++i) {
            const Vec3& a = rect[i];
            const Vec3& b = rect[(i + 1) % rect.size()];
            sb.addVisibleLine(Vec3(X(a.x), Y(a.y), 0), Vec3(X(b.x), Y(b.y), 0));
        }
    }

    // ---- 轴线（点划线）----
    sb.addCenterLine(Vec3(X(-4.0), Y(0), 0), Vec3(X(p.totalL + 4.0), Y(0), 0));

    // ---- 尺寸标注：左侧 4 道竖直直径 ----
    struct VDim { double measureX; double r; int off; QString text; };
    const std::vector<VDim> vdims = {
        { xH + p.flangeT * 0.5,        rF, 10, dimD(p.flangeD) },          // Ø法兰
        { std::max(0.0, xH - 10.0),    rH, 20, dimD(p.hubD) },             // Ø轮毂
        { cd * 0.5,                    rC, 30, dimD(p.cboreD) },           // Ø沉孔
        { (cd + p.totalL) * 0.5,       rB, 40, dimD(p.boreD) + p.boreTol } // Ø通孔
    };
    for (const VDim& d : vdims)
        sb.addDim(DimensionType::Vertical,
                  Vec3(X(d.measureX), Y(-d.r), 0), Vec3(X(d.measureX), Y(d.r), 0),
                  Vec3(X(d.measureX) - d.off, Y(0), 0), d.text);

    // ---- 尺寸标注：下方 3 道水平（沉孔深 / 总长 / 法兰厚）----
    const double yBot = Y(-rF);
    sb.addDim(DimensionType::Horizontal, Vec3(X(0.0), Y(-rC), 0), Vec3(X(cd), Y(-rC), 0),
              Vec3(X(cd * 0.5), yBot - 8.0, 0), n2(p.cboreDepth));
    sb.addDim(DimensionType::Horizontal, Vec3(X(0.0), yBot, 0), Vec3(X(p.totalL), yBot, 0),
              Vec3(X(p.totalL * 0.5), yBot - 16.0, 0), n2(p.totalL));
    sb.addDim(DimensionType::Horizontal, Vec3(X(xH), yBot, 0), Vec3(X(p.totalL), yBot, 0),
              Vec3(X(xH + p.flangeT * 0.5), yBot - 24.0, 0), n2(p.flangeT));

    // ---- 引线标注：螺栓孔规格 / 倒角 / 圆角 ----
    if (!hr.warn && p.boltCount > 0) {
        const double tipX = X(xH + p.flangeT * 0.55);
        const double tipY = Y(p.boltBCD * 0.5 + p.boltD * 0.5);
        const double anX = tipX + 8.0, anY = tipY + 12.0;
        out.push_back(mkLeader(Vec3(tipX, tipY, 0), Vec3(anX, anY, 0), Vec3(anX, anY, 0),
                               QStringLiteral("%1-⌀%2").arg(p.boltCount).arg(n2(p.boltD)),
                               model, p.dimTextHeight));
    }
    if (chf > 1e-6) {
        const double tipX = X(chf * 0.5), tipY = Y(rH - chf * 0.5);
        const double anX = tipX - 10.0, anY = tipY + 10.0;
        out.push_back(mkLeader(Vec3(tipX, tipY, 0), Vec3(anX, anY, 0), Vec3(anX, anY, 0),
                               QStringLiteral("C%1×45°").arg(n2(chf)), model, p.dimTextHeight));
    }
    if (rFilt > 1e-6) {
        // 圆弧中点（−45° 方向的鼓起点）
        const double ox = xH - rFilt, oy = rH + rFilt;
        const double tipX = X(ox + rFilt * 0.7071), tipY = Y(oy - rFilt * 0.7071);
        const double anX = tipX + 6.0, anY = tipY + 12.0;
        out.push_back(mkLeader(Vec3(tipX, tipY, 0), Vec3(anX, anY, 0), Vec3(anX, anY, 0),
                               QStringLiteral("R%1").arg(n2(rFilt)), model, p.dimTextHeight));
    }

    // ---- 粗糙度：孔面（符号尖端落在上孔面、整体旋转 180° 伸入孔内）----
    if (!p.roughBore.isEmpty()) {
        RoughnessSpec rs;
        rs.anchor = Vec3(X(p.totalL * 0.55), Y(rB), 0);
        rs.textHeight = p.dimTextHeight;
        rs.value = p.roughBore;
        rs.rotationDeg = 180.0;
        sb.addRoughness(rs);
        R.roughnessCount += 1;
    }
    if (!p.roughFace.isEmpty()) {
        RoughnessSpec rs2;
        rs2.anchor = Vec3(X(xH + p.flangeT * 0.5), Y(rF), 0);
        rs2.textHeight = p.dimTextHeight;
        rs2.value = p.roughFace;
        sb.addRoughness(rs2);
        R.roughnessCount += 1;
    }

    // ---- 剖视图名 A—A（视图上方居中）----
    Element lbl = buildSectionViewLabel(Vec3(X(p.totalL * 0.5), Y(rF) + 10.0, 0),
                                        QStringLiteral("A"), 5.0, LY_TEXT);
    out.push_back(lbl);

    const SectionViewResult svr = sb.build();
    for (auto& e : svr.elems) out.push_back(std::move(e));
    R.hatchCount += svr.hatchCount;
    R.dimCount   += svr.dimCount;
}

// ---- 左视图（圆形）：圆心 (scx, scy)（图纸坐标）----
void buildSideView(std::vector<Element>& out, const FlangeHubParams& p, double s,
                   double scx, double scy, const SketchModel* model,
                   FlangeHubViewsResult& R) {
    auto circle = [&](double rMM, double w, LineStyle ls, const QColor& c, const QString& name) {
        Element e = ElementFactory::createCircle(Vec3(scx, scy, 0), rMM * s, model);
        e.layer = (ls == LineStyle::DashDot) ? LY_CENTER : LY_OUTLINE;
        e.lineStyle = ls; e.color = c;
        e.lineWidthAuto = false; e.lineWidth = w;
        e.name = name;
        out.push_back(e);
    };
    auto centerLine = [&](const Vec3& a, const Vec3& b, const QString& name) {
        Element e = ElementFactory::createLine(a, b, model);
        e.layer = LY_CENTER; e.lineStyle = LineStyle::DashDot; e.color = C_CENTER;
        e.lineWidthAuto = false; e.lineWidth = 0.5; e.name = name;
        out.push_back(e);
    };

    const double rF = p.flangeR(), rH = p.hubR(), rB = p.boreR(), rC = p.cboreR();

    // 同心圆：法兰外圆 / 轮毂外圆 / 沉孔圆 / 通孔圆（从左侧看均为可见轮廓）
    circle(rF, 2.0, LineStyle::Solid, C_OUTLINE, QStringLiteral("左视图:法兰外圆"));
    circle(rH, 2.0, LineStyle::Solid, C_OUTLINE, QStringLiteral("左视图:轮毂外圆"));
    circle(rC, 2.0, LineStyle::Solid, C_OUTLINE, QStringLiteral("左视图:沉孔圆"));
    circle(rB, 2.0, LineStyle::Solid, C_OUTLINE, QStringLiteral("左视图:通孔圆"));

    // 分布圆（点划线）
    if (p.boltCount > 0)
        circle(p.boltBCD * 0.5, 0.5, LineStyle::DashDot, C_CENTER,
               QStringLiteral("左视图:螺栓分布圆"));
    if (p.locCount > 0)
        circle(p.locBCD * 0.5, 0.5, LineStyle::DashDot, C_CENTER,
               QStringLiteral("左视图:定位分布圆"));

    // 孔：螺栓孔 + 定位孔
    int holes = 0;
    auto drawHoles = [&](int n, double d, double bcd, double a0, const QString& tag) {
        if (n <= 0) return;
        const double rc = bcd * 0.5 * s, rr = d * 0.5 * s;
        for (int i = 0; i < n; ++i) {
            const double a = (a0 + 360.0 * i / n) * K_PI / 180.0;
            const double hx = scx + rc * std::cos(a), hy = scy + rc * std::sin(a);
            Element e = ElementFactory::createCircle(Vec3(hx, hy, 0), rr, model);
            e.layer = LY_OUTLINE; e.lineStyle = LineStyle::Solid; e.color = C_OUTLINE;
            e.lineWidthAuto = false; e.lineWidth = 2.0;
            e.name = QStringLiteral("左视图:%1%2").arg(tag).arg(i + 1);
            out.push_back(e);
            // 孔中心线小十字（长 1.6d，GB/T 4457.4；数量 ≤12 时绘制）
            if (n <= 12) {
                const double cr = rr * 1.6;
                centerLine(Vec3(hx - cr, hy, 0), Vec3(hx + cr, hy, 0),
                           QStringLiteral("左视图:孔中心线"));
                centerLine(Vec3(hx, hy - cr, 0), Vec3(hx, hy + cr, 0),
                           QStringLiteral("左视图:孔中心线"));
            }
            ++holes;
        }
    };
    drawHoles(p.boltCount, p.boltD, p.boltBCD, p.boltAngle0Deg, QStringLiteral("螺栓孔"));
    drawHoles(p.locCount,  p.locD,  p.locBCD,  p.locAngle0Deg,  QStringLiteral("定位孔"));
    R.boltCircleCount = holes;

    // 十字中心线
    const double ext = rF * s + kCenterExt;
    centerLine(Vec3(scx - ext, scy, 0), Vec3(scx + ext, scy, 0),
               QStringLiteral("左视图:水平中心线"));
    centerLine(Vec3(scx, scy - ext, 0), Vec3(scx, scy + ext, 0),
               QStringLiteral("左视图:垂直中心线"));

    // 剖切符号 A—A：竖直剖切线（穿轴线，即螺栓孔所在平面），字母注在上下两端
    const double c0 = scy - rF * s - 4.0, c1 = scy + rF * s + 4.0;
    auto cut = buildCutAnnotation(Vec3(scx, c0, 0), Vec3(scx, c1, 0),
                                  CutViewDir::Right, QStringLiteral("A"),
                                  5.0, LY_DIM);
    R.cutSymbolCount = (int)cut.size();
    for (auto& e : cut) out.push_back(std::move(e));
}

} // namespace

// ============================================================================
// 参数合法性收敛
// ============================================================================
QString FlangeHubParams::validate() const {
    if (flangeD <= 0.0 || hubD <= 0.0 || cboreD <= 0.0 || boreD <= 0.0)
        return QStringLiteral("径向尺寸必须为正");
    if (!(flangeD > hubD && hubD > cboreD && cboreD > boreD))
        return QStringLiteral("径向尺寸须满足 法兰>轮毂>沉孔>通孔（不变式1）");
    if (flangeT <= 0.0 || totalL <= 0.0 || cboreDepth <= 0.0)
        return QStringLiteral("轴向尺寸必须为正");
    if (totalL <= flangeT)
        return QStringLiteral("总长必须大于法兰厚度");
    if (cboreDepth > hubLen())
        return QStringLiteral("沉孔深不得伸进法兰段（不变式2）");
    if (chamfer < 0.0 || chamfer >= hubR() - cboreR())
        return QStringLiteral("倒角过大：左端面倒角后须保留环带（不变式3）");
    if (filletR < 0.0 || filletR >= std::min(flangeT, flangeR() - hubR()))
        return QStringLiteral("圆角过大：须落在法兰根部实体内（不变式4）");
    if (boltCount < 0 || boltCount > 36) return QStringLiteral("螺栓孔数量非法（0~36）");
    if (locCount  < 0 || locCount  > 36) return QStringLiteral("定位孔数量非法（0~36）");
    auto holeOk = [](int n, double d, double bcd, double rIn, double rOut) -> bool {
        if (n <= 0) return true;
        // 孔整圆须落在 (rIn, rOut) 环带内，距两边界各留 0.5mm 余量
        return (bcd * 0.5 - d * 0.5 > rIn + 0.5) && (bcd * 0.5 + d * 0.5 < rOut - 0.5);
    };
    if (!holeOk(boltCount, boltD, boltBCD, boreR(), flangeR()))
        return QStringLiteral("螺栓孔未完整落在通孔与法兰外缘之间（不变式5）");
    if (!holeOk(locCount, locD, locBCD, boreR(), flangeR()))
        return QStringLiteral("定位孔未完整落在通孔与法兰外缘之间（不变式5）");
    if (boltD <= 0.0 || locD <= 0.0) return QStringLiteral("孔径必须为正");
    return QString();
}

void FlangeHubParams::makeValid() {
    const auto clamp = [](double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); };
    flangeD = clamp(flangeD, 20.0, 2000.0);
    cboreD  = clamp(cboreD,  2.0, flangeD * 0.8);
    boreD   = clamp(boreD,   1.0, cboreD - 1.0);
    hubD    = clamp(hubD, cboreD + 2.0, flangeD - 0.5);       // 不变式1
    flangeT = clamp(flangeT, 2.0, totalL * 0.9);
    totalL  = clamp(totalL, flangeT + 5.0, 3000.0);
    flangeT = clamp(flangeT, 2.0, totalL * 0.9);
    cboreDepth = clamp(cboreDepth, 1.0, std::max(1.0, hubLen()));
    chamfer = clamp(chamfer, 0.0, std::max(0.0, (hubR() - cboreR()) * 0.9));   // 不变式3
    filletR = clamp(filletR, 0.0, std::max(0.0, std::min(flangeT, flangeR() - hubR()) * 0.9)); // 不变式4
    boltCount = std::max(0, std::min(36, boltCount));
    locCount  = std::max(0, std::min(36, locCount));
    boltD = clamp(boltD, 1.0, flangeD * 0.3);
    locD  = clamp(locD,  1.0, flangeD * 0.3);
    auto fitHoles = [&](int n, double& d, double& bcd) {
        if (n <= 0) return;
        const double rIn  = boreR() + 0.5;        // 环带内边界（留 0.5mm 余量）
        const double rOut = flangeR() - 0.5;       // 环带外边界
        if (rOut <= rIn + d) {                    // 环带甚至放不下一个孔：先压孔径，再取环带中点
            d = clamp((rOut - rIn) * 0.4, 1.0, d);
            bcd = rIn + rOut;                     // 半径中点 *2
        } else {
            const double lo = 2.0 * (rIn + d * 0.5 + 0.25);
            const double hi = 2.0 * (rOut - d * 0.5 - 0.25);
            bcd = clamp(bcd, lo, hi);
        }
    };
    fitHoles(boltCount, boltD, boltBCD);   // 不变式5
    fitHoles(locCount,  locD,  locBCD);    // 不变式5
    dimTextHeight = clamp(dimTextHeight, 2.0, 7.0);
}

double FlangeHubParams::viewScale() const {
    FlangeHubParams q = *this; q.makeValid();
    if (!autoScale) return manualScale;
    const QRectF a = q.availableArea();
    for (double cand : kStdScales) {
        const double needW = kLeftBand + q.totalL * cand + kRightPad + q.gap
                             + q.flangeD * cand + 2.0 * kCenterExt;
        const double needH = kTopPad + q.flangeD * cand + kBotBand;
        if (needW <= a.width() + 1e-9 && needH <= a.height() + 1e-9) return cand;
    }
    return kStdScales[sizeof(kStdScales) / sizeof(kStdScales[0]) - 1];
}

QRectF FlangeHubParams::availableArea() const {
    const double x0 = marginL, x1 = sheetW - marginC;
    const double y0 = marginC, y1 = sheetH - marginC;
    QRectF full(x0, y0, std::max(20.0, x1 - x0), std::max(20.0, y1 - y0));
    const double tbH = std::min(titleBlockH, full.height() * 0.5);
    // 标题栏在右下角：取「标题栏上方整幅」与「标题栏左侧整幅」中面积较大者（同 HousingViews）
    QRectF above(x0, y0 + tbH + gap, full.width(),
                 std::max(20.0, full.height() - (tbH + gap)));
    QRectF left(full.left(), y0, std::max(20.0, full.width() - (titleBlockW + gap)),
                full.height());
    return (above.width() * above.height() >= left.width() * left.height()) ? above : left;
}

QRectF FlangeHubParams::viewsBounds() const {
    FlangeHubParams q = *this; q.makeValid();
    const double s = q.viewScale();
    const double w = kLeftBand + q.totalL * s + kRightPad + q.gap
                     + q.flangeD * s + 2.0 * kCenterExt;
    const double h = kTopPad + q.flangeD * s + kBotBand;
    return QRectF(0, 0, w, h);
}

// ============================================================================
// 视图生成
// ============================================================================
FlangeHubViewsResult buildFlangeHubViews(const FlangeHubParams& pin, const SketchModel* model) {
    FlangeHubParams p = pin;
    p.makeValid();
    const double s = p.viewScale();
    const QRectF area = p.availableArea();
    const double ax0 = area.left(), ay0 = area.top();               // 左 / 下
    const double ay1 = ay0 + area.height();                          // 上

    FlangeHubViewsResult R;
    R.scale = s;
    R.scaleLabel = scaleLabel(s);
    R.area = area;

    const double rF = p.flangeR();
    const double frontHalfL = p.totalL * 0.5 * s;
    const double sideHalfR  = rF * s;

    // ---- 锚点布置：主视图居左、左视图居右，轴线等高 ----
    const double axisY = ay1 - kTopPad - sideHalfR;
    const double fx0   = ax0 + kLeftBand;                    // 零件左端 x
    const double frontCx = fx0 + frontHalfL;
    const double sideCx  = fx0 + p.totalL * s + kRightPad + p.gap + sideHalfR;
    const double sideCy  = axisY;

    R.frontCx = frontCx; R.frontCy = axisY;
    R.sideCx = sideCx;   R.sideCy = sideCy;
    R.frontHalfL = frontHalfL;
    R.sideR = sideHalfR;

    // ---- 两个视图 ----
    buildFrontSection(R.elems, p, s, fx0, axisY, model, R);
    buildSideView(R.elems, p, s, sideCx, sideCy, model, R);

    // ---- 技术要求（可用区左下角）----
    {
        const double tx = ax0 + 2.0;
        const double ty = ay0 + kBotBand + 14.0;
        R.elems.push_back([&]{
            Element e = ElementFactory::createText(Vec3(tx, ty, 0),
                QStringLiteral("技术要求"), 5.0);
            e.layer = LY_TEXT; e.color = C_OUTLINE; e.name = QStringLiteral("技术要求标题");
            return e;
        }());
        const QString lines[] = {
            QStringLiteral("1. 未注倒角 C1。"),
            QStringLiteral("2. 未注圆角 R3。"),
            QStringLiteral("3. 调质处理 220~250HBW。"),
        };
        for (int i = 0; i < 3; ++i) {
            Element e = ElementFactory::createText(Vec3(tx, ty - 7.0 * (i + 1), 0),
                lines[i], 3.5);
            e.layer = LY_TEXT; e.color = C_OUTLINE; e.name = QStringLiteral("技术要求");
            R.elems.push_back(e);
        }
    }

    // ---- 「其余」粗糙度（图纸右上角图框内侧）----
    {
        RoughnessSpec rs;
        rs.anchor = Vec3(p.sheetW - p.marginC - 8.0, p.sheetH - p.marginC - 12.0, 0);
        rs.textHeight = 3.5;
        rs.value = QStringLiteral("其余");
        rs.layer = LY_DIM;
        auto syms = buildRoughnessSymbol(rs);
        for (auto& e : syms) R.elems.push_back(std::move(e));
        R.roughnessCount += 1;
    }

    // ---- 规格摘要（可用区左上角）----
    {
        const QString note = QStringLiteral("轮毂法兰 %1 | 孔 %2-⌀%3 | 比例 %4")
                                 .arg(dimD(p.flangeD)).arg(p.boltCount).arg(n2(p.boltD))
                                 .arg(R.scaleLabel);
        Element e = ElementFactory::createText(Vec3(ax0, ay1 - 4.0, 0), note, 3.5);
        e.layer = LY_TEXT; e.color = C_OUTLINE; e.name = QStringLiteral("说明文字");
        R.elems.push_back(e);
    }

    // ---- 统计与包围盒 ----
    for (const auto& e : R.elems)
        if (e.type == ElementType::Dimension) ++R.dimCount;

    QRectF b; bool has = false;
    for (const auto& e : R.elems) {
        const QRectF eb = geo::elementBounds(e);
        b = has ? b.united(eb) : eb; has = true;
    }
    R.bounds = b;
    R.fits = area.contains(b);
    return R;
}

QString flangeHubViewTitle(int index) {
    switch (index) {
        case 0: return QStringLiteral("主视图（全剖 A—A）");
        case 1: return QStringLiteral("左视图");
    }
    return QStringLiteral("视图");
}

} // namespace SketchCAD
