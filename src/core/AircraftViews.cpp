#include "AircraftViews.h"
#include "ElementFactory.h"
#include "SketchModel.h"
#include <cmath>
#include <algorithm>
#include <QColor>

namespace SketchCAD {
namespace aircraft {

namespace {

constexpr double M_TO_FT = 3.280839895013123;

QString dimText(double m) {
    const double ft = m * M_TO_FT;
    return QStringLiteral("%1 m\n(%2 ft)")
        .arg(m, 0, 'f', 2)
        .arg(ft, 0, 'f', 2);
}

Element makeLine(const Vec3& a, const Vec3& b, const SketchModel* model) {
    return ElementFactory::createLine(a, b, model);
}

Element makeCircle(const Vec3& c, double r, const SketchModel* model) {
    return ElementFactory::createCircle(c, r, model);
}

Element makeArc(const Vec3& c, double r, double s0, double s1,
                const SketchModel* model) {
    return ElementFactory::createArc(c, r, s0, s1, model);
}

Element makeDim(DimensionType dt, const Vec3& a, const Vec3& b,
                const Vec3& pos, const QString& text,
                const SketchModel* model) {
    Element e = ElementFactory::createDimension(dt, model);
    e.dtype = dt;
    e.pts = {a, b, pos};
    e.text = text;
    return e;
}

Element makeText(const Vec3& pos, const QString& text, double h) {
    return ElementFactory::createText(pos, text, h);
}

// 投影对应辅助线（细虚线，浅灰）——仅用于表达视图之间的投影对应关系，
// 不参与实体轮廓，渲染时以细线呈现，便于核对"长/宽/高"三向投影对齐。
Element makeGuide(const Vec3& a, const Vec3& b, const SketchModel* model) {
    Element e = makeLine(a, b, model);
    e.lineStyle = LineStyle::Dashed;
    e.color = QColor(110, 120, 140);
    e.lineWidthAuto = false;
    e.lineWidth = 0.5;
    return e;
}

// 中心线（点划线，红）——GB/T 4457 对称/轴线规范，用于标注各视图的轴线与对称中心。
Element makeCenterLine(const Vec3& a, const Vec3& b, const SketchModel* model) {
    Element e = makeLine(a, b, model);
    e.lineStyle = LineStyle::DashDot;
    e.color = QColor(170, 60, 60);
    e.lineWidthAuto = false;
    e.lineWidth = 0.5;
    return e;
}

// ---- 侧面图 ----
void buildSideView(std::vector<Element>& out, const AircraftViewsParams& p,
                   const Vec3& o, const SketchModel* model) {
    const double h  = p.fuselageHeight;
    const double r  = h * 0.5;                 // 机头圆弧半径
    const double te = p.length * 0.18;         // 尾锥长度
    const double bodyEnd = p.length - te;
    const double tailExt = std::max(p.tailHeight - h * 0.5, 0.0) * 0.7;

    // 机身主体上下两条线
    out.push_back(makeLine(o + Vec3(r, h), o + Vec3(bodyEnd, h), model));
    out.push_back(makeLine(o + Vec3(r, 0), o + Vec3(bodyEnd, 0), model));

    // 机头半圆
    out.push_back(makeArc(o + Vec3(r, h * 0.5), r, 90.0, 270.0, model));

    // 尾锥
    out.push_back(makeLine(o + Vec3(bodyEnd, h), o + Vec3(p.length, h * 0.5), model));
    out.push_back(makeLine(o + Vec3(bodyEnd, 0), o + Vec3(p.length, h * 0.5), model));

    // 垂尾
    if (tailExt > 0.1) {
        const double root0 = bodyEnd + te * 0.2;
        const double root1 = bodyEnd + te * 0.6;
        out.push_back(makeLine(o + Vec3(root0, h), o + Vec3(root1, h), model));
        out.push_back(makeLine(o + Vec3(root0, h), o + Vec3(p.length, h + tailExt), model));
        out.push_back(makeLine(o + Vec3(root1, h), o + Vec3(p.length, h + tailExt), model));
    }

    // 发动机（侧面用圆表示）
    out.push_back(makeCircle(o + Vec3(p.engineX, h * 0.5), h * 0.22, model));

    // 驾驶舱示意
    out.push_back(makeLine(o + Vec3(r * 1.1, h * 0.72),
                           o + Vec3(r * 2.2, h * 0.82), model));
    out.push_back(makeLine(o + Vec3(r * 2.2, h * 0.82),
                           o + Vec3(r * 3.0, h * 0.75), model));

    const double dOff = 5.0;   // 尺寸线偏移

    // 总长 38.90 m
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(0, h + dOff),
                          o + Vec3(p.length, h + dOff),
                          o + Vec3(p.length * 0.5, h + dOff),
                          dimText(p.length), model));

    // 机高 6.13 m
    out.push_back(makeDim(DimensionType::Vertical,
                          o + Vec3(0, 0),
                          o + Vec3(0, h),
                          o + Vec3(-dOff, h * 0.5),
                          dimText(p.fuselageHeight), model));

    // 发动机距机头 12.08 m
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(0, 0),
                          o + Vec3(p.engineX, 0),
                          o + Vec3(p.engineX * 0.5, -dOff),
                          dimText(p.engineX), model));
}

// ---- 俯视图 ----
void buildTopView(std::vector<Element>& out, const AircraftViewsParams& p,
                  const Vec3& o, const SketchModel* model) {
    const double cy = p.wingspan * 0.5;          // 机身中心线 y
    const double h  = p.fuselageHeight;          // 俯视机身半宽近似
    const double r  = h * 0.5;
    const double te = p.length * 0.15;
    const double bodyEnd = p.length - te;

    // 机身两侧线
    out.push_back(makeLine(o + Vec3(r, cy + r),
                           o + Vec3(bodyEnd, cy + r), model));
    out.push_back(makeLine(o + Vec3(r, cy - r),
                           o + Vec3(bodyEnd, cy - r), model));

    // 机头圆弧
    out.push_back(makeArc(o + Vec3(r, cy), r, 90.0, 270.0, model));

    // 尾锥
    out.push_back(makeLine(o + Vec3(bodyEnd, cy + r),
                           o + Vec3(p.length, cy), model));
    out.push_back(makeLine(o + Vec3(bodyEnd, cy - r),
                           o + Vec3(p.length, cy), model));

    // 垂尾投影
    out.push_back(makeLine(o + Vec3(bodyEnd + te * 0.3, cy + r * 0.4),
                           o + Vec3(p.length, cy + r * 1.2), model));
    out.push_back(makeLine(o + Vec3(bodyEnd + te * 0.3, cy - r * 0.4),
                           o + Vec3(p.length, cy - r * 1.2), model));

    // 机翼（梯形）
    const double rootX = std::max(p.engineX - 2.0, r * 1.5);
    const double tipX  = p.length * 0.82;
    out.push_back(makeLine(o + Vec3(rootX, cy + r),
                           o + Vec3(tipX, cy + p.wingspan * 0.5), model));
    out.push_back(makeLine(o + Vec3(rootX, cy - r),
                           o + Vec3(tipX, cy - p.wingspan * 0.5), model));
    out.push_back(makeLine(o + Vec3(rootX, cy + r),
                           o + Vec3(tipX * 0.95, cy + p.wingspan * 0.42), model));
    out.push_back(makeLine(o + Vec3(rootX, cy - r),
                           o + Vec3(tipX * 0.95, cy - p.wingspan * 0.42), model));

    // 发动机
    out.push_back(makeCircle(o + Vec3(p.engineX, cy + p.engineY), h * 0.30, model));
    out.push_back(makeCircle(o + Vec3(p.engineX, cy - p.engineY), h * 0.30, model));

    // 起落架
    out.push_back(makeCircle(o + Vec3(p.noseGearX, cy), 0.5, model));
    out.push_back(makeCircle(o + Vec3(p.mainGearX, cy + p.mainGearTrack * 0.5), 0.6, model));
    out.push_back(makeCircle(o + Vec3(p.mainGearX, cy - p.mainGearTrack * 0.5), 0.6, model));

    const double dOff = 5.0;

    // 翼展 35.80 m
    out.push_back(makeDim(DimensionType::Vertical,
                          o + Vec3(0, cy - p.wingspan * 0.5),
                          o + Vec3(0, cy + p.wingspan * 0.5),
                          o + Vec3(-dOff, cy),
                          dimText(p.wingspan), model));

    // 发动机 lateral 12.60 m
    out.push_back(makeDim(DimensionType::Vertical,
                          o + Vec3(p.length, cy - p.engineY),
                          o + Vec3(p.length, cy + p.engineY),
                          o + Vec3(p.length + dOff, cy),
                          dimText(p.engineY * 2.0), model));

    // 主起落架轮距 7.62 m
    out.push_back(makeDim(DimensionType::Vertical,
                          o + Vec3(p.mainGearX, cy - p.mainGearTrack * 0.5),
                          o + Vec3(p.mainGearX, cy + p.mainGearTrack * 0.5),
                          o + Vec3(p.mainGearX + dOff, cy),
                          dimText(p.mainGearTrack), model));
}

// ---- 正面图 ----
void buildFrontView(std::vector<Element>& out, const AircraftViewsParams& p,
                    const Vec3& o, const SketchModel* model) {
    const double cx = p.wingspan * 0.5;
    const double h  = p.fuselageHeight;
    const double cy = h * 0.5;
    const double tailExt = std::max(p.tailHeight - h * 0.5, 0.0) * 0.7;

    // 机身（圆）
    out.push_back(makeCircle(o + Vec3(cx, cy), h * 0.5, model));

    // 垂尾
    if (tailExt > 0.1) {
        const double hw = h * 0.35;
        out.push_back(makeLine(o + Vec3(cx - hw, h), o + Vec3(cx + hw, h), model));
        out.push_back(makeLine(o + Vec3(cx - hw, h), o + Vec3(cx, h + tailExt), model));
        out.push_back(makeLine(o + Vec3(cx + hw, h), o + Vec3(cx, h + tailExt), model));
    }

    // 机翼
    out.push_back(makeLine(o + Vec3(cx - h * 0.5, cy),
                           o + Vec3(0, cy), model));
    out.push_back(makeLine(o + Vec3(cx + h * 0.5, cy),
                           o + Vec3(p.wingspan, cy), model));

    // 发动机
    out.push_back(makeCircle(o + Vec3(cx - p.engineY, cy), h * 0.28, model));
    out.push_back(makeCircle(o + Vec3(cx + p.engineY, cy), h * 0.28, model));

    // 起落架
    out.push_back(makeCircle(o + Vec3(cx - p.mainGearTrack * 0.5, 0), 0.6, model));
    out.push_back(makeCircle(o + Vec3(cx + p.mainGearTrack * 0.5, 0), 0.6, model));
    out.push_back(makeCircle(o + Vec3(cx, 0), 0.5, model));

    const double dOff = 5.0;
    const double top = h + tailExt;

    // 翼展 35.80 m
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(0, top + dOff),
                          o + Vec3(p.wingspan, top + dOff),
                          o + Vec3(cx, top + dOff),
                          dimText(p.wingspan), model));

    // 发动机 lateral 12.60 m（半翼展 * 2）
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(cx - p.engineY, top + 2.5 * dOff),
                          o + Vec3(cx + p.engineY, top + 2.5 * dOff),
                          o + Vec3(cx, top + 2.5 * dOff),
                          dimText(p.engineY * 2.0), model));

    // 主起落架轮距 7.62 m
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(cx - p.mainGearTrack * 0.5, -dOff),
                          o + Vec3(cx + p.mainGearTrack * 0.5, -dOff),
                          o + Vec3(cx, -dOff),
                          dimText(p.mainGearTrack), model));

    // 底部外廓 8.98 m
    out.push_back(makeDim(DimensionType::Horizontal,
                          o + Vec3(cx - p.outerWidth * 0.5, -2.5 * dOff),
                          o + Vec3(cx + p.outerWidth * 0.5, -2.5 * dOff),
                          o + Vec3(cx, -2.5 * dOff),
                          dimText(p.outerWidth), model));
}

} // namespace

std::vector<Element> buildThreeViews(const AircraftViewsParams& p,
                                     const SketchModel* model) {
    std::vector<Element> out;
    out.reserve(96);

    // 各视图在"局部坐标"中的高度（统一比例 1 m = 1 单位，保证三面图比例协调）。
    const double sideH  = std::max(p.fuselageHeight,
                                  p.fuselageHeight +
                                      std::max(p.tailHeight - p.fuselageHeight * 0.5,
                                               0.0) * 0.7);
    const double topH   = p.wingspan;   // 俯视图竖直方向 = 翼展
    const double frontH = sideH;        // 正面图与侧视图等高（共享机身高度）
    const double gap    = 12.0;         // 视图间距（含投影/标注余量）

    // ===== 视图布局（按机械制图投影规范）=====
    //   上方：侧视图；右侧：正面图；下方：俯视图。
    //   · 侧视图与俯视图在长度方向(x=机身长度)上严格对齐 → 垂直投影对应；
    //   · 侧视图与正面图在高度方向(y=机身高度)上基线对齐、等高 → 水平投影对应。
    Vec3 topO(0.0, 0.0, 0.0);                    // 俯视图：下方
    Vec3 sideO(0.0, topH + gap, 0.0);            // 侧视图：上方
    Vec3 frontO(p.length + gap, sideO.y, 0.0);   // 正面图：右侧（与侧视图等高、基线对齐）

    // 视图标题
    const double titleH = 4.0;
    out.push_back(makeText(sideO + Vec3(p.length * 0.5, sideH + 2.0, 0),
                           QStringLiteral("侧面图"), titleH));
    out.push_back(makeText(topO  + Vec3(p.length * 0.5, topH + 2.0, 0),
                           QStringLiteral("俯视图"), titleH));
    out.push_back(makeText(frontO + Vec3(p.wingspan * 0.5, frontH + 2.0, 0),
                           QStringLiteral("正面图"), titleH));

    buildSideView (out, p, sideO, model);
    buildTopView  (out, p, topO,  model);
    buildFrontView(out, p, frontO, model);

    // ===== 投影对应辅助线（细虚线，仅表达视图间投影关系）=====
    // 侧↔俯：沿长度方向（机头 x=0、机尾 x=length）在二者间隙投影。
    out.push_back(makeGuide(Vec3(0.0,      topO.y + topH, 0), Vec3(0.0,      sideO.y, 0), model));
    out.push_back(makeGuide(Vec3(p.length, topO.y + topH, 0), Vec3(p.length, sideO.y, 0), model));
    // 侧↔正：沿高度方向（基线 y=sideO.y、机顶 y=sideO.y+sideH）在二者间隙投影。
    out.push_back(makeGuide(Vec3(p.length, sideO.y,         0), Vec3(frontO.x, sideO.y,         0), model));
    out.push_back(makeGuide(Vec3(p.length, sideO.y + sideH, 0), Vec3(frontO.x, sideO.y + sideH, 0), model));

    // ===== 中心线（点划线，GB/T 4457 对齐规范）=====
    const double ce = 3.0; // 中心线两端伸出量
    // 侧视图：机身纵向中心线（水平，过机身半高）
    out.push_back(makeCenterLine(Vec3(-ce, sideO.y + p.fuselageHeight * 0.5, 0),
                                 Vec3(p.length + ce, sideO.y + p.fuselageHeight * 0.5, 0), model));
    // 俯视图：机身纵向中心线（水平，过翼展半宽）+ 翼展方向中心线（竖直）
    out.push_back(makeCenterLine(Vec3(-ce, topO.y + p.wingspan * 0.5, 0),
                                 Vec3(p.length + ce, topO.y + p.wingspan * 0.5, 0), model));
    out.push_back(makeCenterLine(Vec3(p.length * 0.5, -ce, 0),
                                 Vec3(p.length * 0.5, p.wingspan + ce, 0), model));
    // 正面图：翼展/机身竖直中心线（过翼展半宽）
    out.push_back(makeCenterLine(Vec3(frontO.x + p.wingspan * 0.5, frontO.y - ce, 0),
                                 Vec3(frontO.x + p.wingspan * 0.5, frontO.y + frontH + ce, 0), model));

    return out;
}

} // namespace aircraft
} // namespace SketchCAD
