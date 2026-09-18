#include "BearingViews.h"
#include "ElementFactory.h"
#include "Geometry.h"
#include "SketchModel.h"
#include "Section.h"
#include "SheetSpec.h"
#include "DimStyle.h"
#include <QColor>
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace {

// ----------------------------- 图层与线型约定 -----------------------------
const QString LY_OUTLINE = QStringLiteral("轴承-轮廓");   // 粗实线：可见轮廓
const QString LY_HIDDEN  = QStringLiteral("轴承-虚线");   // 细虚线：不可见轮廓
const QString LY_CENTER  = QStringLiteral("轴承-中心线"); // 点划线：轴线
const QString LY_DIM     = QStringLiteral("轴承-标注");   // 尺寸标注
const QString LY_SECT    = QStringLiteral("轴承-剖面");   // 断面（Hatch）

const QColor C_OUTLINE(26, 29, 40);    // 深灰（粗实线）
const QColor C_HIDDEN (110, 120, 140); // 浅灰（虚线）
const QColor C_CENTER (170, 60, 60);   // 暗红（中心线）

constexpr double K_PI  = 3.14159265358979323846;
constexpr double W_OUT = 2.0;   // 粗实线
constexpr double W_HID = 1.0;   // 细虚线
constexpr double W_CEN = 0.5;   // 中心线
constexpr double W_HAT = 2.0;   // 断面外轮廓（填充后不变细）

Element mkLine(const Vec3& a, const Vec3& b, const QString& layer,
               LineStyle ls, const QColor& col, double w,
               const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createLine(a, b, m);
    e.layer = layer; e.lineStyle = ls; e.color = col;
    e.lineWidthAuto = false; e.lineWidth = w;
    e.name = name;
    return e;
}
Element solid(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, W_OUT, name, m);
}
Element hidden(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_HIDDEN, LineStyle::Dashed, C_HIDDEN, W_HID, name, m);
}
Element centerLine(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_CENTER, LineStyle::DashDot, C_CENTER, W_CEN, name, m);
}
Element mkCircle(const Vec3& c, double r, const QString& layer, LineStyle ls,
                 const QColor& col, double w, const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createCircle(c, r, m);
    e.layer = layer; e.lineStyle = ls; e.color = col;
    e.lineWidthAuto = false; e.lineWidth = w; e.name = name;
    return e;
}
Element mkArc(const Vec3& c, double r, double a0, double a1,
              const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createArc(c, r, a0, a1, m);
    e.layer = LY_OUTLINE; e.lineStyle = LineStyle::Solid; e.color = C_OUTLINE;
    e.lineWidthAuto = false; e.lineWidth = W_OUT; e.name = name;
    return e;
}
// 矩形轮廓（四条边，粗实线）
void mkRect(std::vector<Element>& out, const Vec3& lo, const Vec3& hi,
            const QString& name, const SketchModel* m) {
    out.push_back(solid(lo, Vec3(hi.x, lo.y, 0), name + QStringLiteral("·下"), m));
    out.push_back(solid(Vec3(hi.x, lo.y, 0), hi, name + QStringLiteral("·右"), m));
    out.push_back(solid(hi, Vec3(lo.x, hi.y, 0), name + QStringLiteral("·上"), m));
    out.push_back(solid(Vec3(lo.x, hi.y, 0), lo, name + QStringLiteral("·左"), m));
}
Element mkDim(DimensionType dt, const Vec3& a, const Vec3& b, const Vec3& pos,
              const QString& text, const SketchModel* m, double dh) {
    Element e = ElementFactory::createDimension(dt, m);
    e.dtype = dt; e.pts = {a, b, pos}; e.text = text;
    e.layer = LY_DIM; e.name = QStringLiteral("尺寸标注"); e.dimTextHeight = dh;
    return e;
}

// 剖面线间距：GB/T 4457.5 规定 2~4mm（最小 0.7mm）。
// 断面在图纸上很薄时按比例缩小，保证至少能画出 3 条剖面线。
double hatchSpacingFor(const std::vector<Vec3>& poly) {
    double xmin = 1e18, xmax = -1e18, ymin = 1e18, ymax = -1e18;
    for (const auto& p : poly) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    }
    const double t = std::min(xmax - xmin, ymax - ymin);
    if (t < 9.0) return std::max(0.7, t / 3.0);
    return 3.0;
}
// 由断面多边形生成 Hatch 图元（GB/T 4457.5-2013，45° 单一族细实线）。
Element makeBearingHatch(const std::vector<Vec3>& boundary, const QString& name) {
    SectionRegion r;
    r.boundary = boundary;
    r.style.angleDeg = 45.0;
    r.style.spacingMm = hatchSpacingFor(boundary);
    r.style.material = SectionMaterial::General;   // 同一零件：同方向、同间隔
    Element h = makeHatchElement(r, name);
    h.layer = LY_SECT; h.color = C_OUTLINE; h.lineWidth = W_HAT; h.lineWidthAuto = false;
    return h;
}

QString n2(double v) {
    const double r = std::round(v);
    if (std::fabs(v - r) < 1e-9) return QString::number((long long)r);
    return QString::number(v, 'f', 1);
}
QString dimD(double v) { return QStringLiteral("⌀") + n2(v); }

const double kStdScales[] = {50.0, 20.0, 10.0, 5.0, 2.0, 1.0,
                             1.0 / 2.0, 1.0 / 2.5, 1.0 / 5.0, 1.0 / 10.0,
                             1.0 / 20.0, 1.0 / 50.0, 1.0 / 100.0};
QString scaleLabel(double s) {
    if (std::fabs(s - 1.0) < 1e-9) return QStringLiteral("1:1");
    if (s > 1.0) return QStringLiteral("%1:1").arg(n2(s));
    return QStringLiteral("1:%1").arg(n2(1.0 / s));
}

constexpr double kDimPad = 10.0;    // 尺寸标注外溢预留（图纸 mm）
double modelBlockW(const BearingParams& q) { return q.baseL + q.baseW; }
double modelBlockH(const BearingParams& q) { return q.overallH + q.baseW; }

double pickScale(const BearingParams& q, const QRectF& area, double gapH, double gapV) {
    // 模型尺寸(mm) × 候选比例 + 不随比例变化的图纸间隙 + 标注预留 ≤ 可用区。
    const double mw = modelBlockW(q), mh = modelBlockH(q);
    const double safeW = std::max(1.0, area.width()  - 2.0 * kDimPad);
    const double safeH = std::max(1.0, area.height() - 2.0 * kDimPad);
    for (double cand : kStdScales)
        if (mw * cand + gapH <= safeW + 1e-9 && mh * cand + gapV <= safeH + 1e-9) return cand;
    return kStdScales[sizeof(kStdScales) / sizeof(kStdScales[0]) - 1];
}

} // namespace

// ============================================================================
QString BearingParams::validate() const {
    if (bossD <= 0 || ringD <= 0 || boreD <= 0) return QStringLiteral("轴承孔直径必须为正");
    if (ringD >= bossD)  return QStringLiteral("轴承孔必须小于圆筒外径");
    if (boreD >= ringD)  return QStringLiteral("台阶孔必须小于轴承孔");
    if (bossLen <= 0)    return QStringLiteral("圆筒长度必须为正");
    if (baseL <= 0 || baseW <= 0 || baseThk <= 0) return QStringLiteral("底板尺寸必须为正");
    if (overallH <= 0)   return QStringLiteral("总高必须为正");
    if (webBotW <= 0)    return QStringLiteral("支承板底宽必须为正");
    if (bossLen > baseW + 1e-9) return QStringLiteral("圆筒长度不得大于底板宽度");
    if (webTopY() <= baseThk + 1e-9) return QStringLiteral("轴承孔底必须高于底板顶面");
    if (footSlotWd <= 0 || footSlotDepth <= 0) return QStringLiteral("地脚槽尺寸必须为正");
    if (footSlotWd >= baseW - 4.0) return QStringLiteral("地脚槽宽度超出底板");
    if (footSlotDepth < footSlotWd * 0.5 + 4.0)
        return QStringLiteral("地脚槽深度须容纳槽底半圆");
    if (slotCx() - footSlotWd * 0.5 < webBotW * 0.5)
        return QStringLiteral("地脚槽切入支承板正投影");
    return QString();
}

void BearingParams::makeValid() {
    const auto clamp = [](double v, double lo, double hi) {
        return std::max(lo, std::min(hi, v));
    };
    baseL   = clamp(baseL, 40.0, 5000.0);
    baseW   = clamp(baseW, 20.0, baseL);
    baseThk = clamp(baseThk, 4.0, 1000.0);
    bossD   = clamp(bossD, 20.0, 5000.0);
    ringD   = clamp(ringD, 10.0, bossD - 2.0);
    boreD   = clamp(boreD, 4.0, ringD - 2.0);
    bossLen = clamp(bossLen, 10.0, baseW);
    // 地脚 U 形槽收敛：槽底半圆能放下，且不切入支承板正投影
    footSlotWd    = clamp(footSlotWd, 4.0, std::max(4.0, baseW - 6.0));
    footSlotDepth = clamp(footSlotDepth, footSlotWd * 0.5 + 4.0, baseL * 0.5 - 4.0);
    webBotW = std::min(webBotW, std::max(16.0, 2.0 * (slotCx() - footSlotWd * 0.5 - 1.0)));
    webBotW = clamp(webBotW, 16.0, baseL - 10.0);
    // 不变式 2：轴承孔底必须高于底板顶面（否则抬高总高）
    const double need = baseThk + ringD * 0.5 + bossD * 0.5 + 4.0;
    overallH = clamp(overallH, need, 20000.0);
}

// 图纸坐标：原点在图纸左下角，Y 轴向上（与 GraphicsView 一致）。
QRectF BearingParams::availableArea() const {
    const double x0 = marginL, x1 = sheetW - marginC;
    const double y0 = marginC,  y1 = sheetH - marginC;
    QRectF full(x0, y0, std::max(20.0, x1 - x0), std::max(20.0, y1 - y0));
    const double tbH = std::min(titleBlockH, full.height() * 0.5);
    const double tbW = std::min(titleBlockW, full.width() * 0.7);
    // 标题栏固定在右下角：绘图区取「标题栏上方整幅」与「标题栏左侧整幅」中面积较大者。
    QRectF above(x0, y0 + tbH + gap, full.width(),
                 std::max(20.0, full.height() - (tbH + gap)));
    QRectF left(full.left(), y0, std::max(20.0, full.width() - (tbW + gap)), full.height());
    return (above.width() * above.height() >= left.width() * left.height()) ? above : left;
}

double BearingParams::viewScale() const {
    BearingParams q = *this; q.makeValid();
    const QRectF area = q.availableArea();
    const double gapH = std::max(gap, 8.0);
    const double gapV = gapH + 8.0;
    return autoScale ? pickScale(q, area, gapH, gapV) : manualScale;
}

QRectF BearingParams::viewsBounds() const {
    BearingParams q = *this; q.makeValid();
    const double s = viewScale();
    const double gapH = std::max(gap, 8.0);
    const double gapV = gapH + 8.0;
    return QRectF(0, 0, modelBlockW(q) * s + gapH + 2.0 * kDimPad,
                  modelBlockH(q) * s + gapV + 2.0 * kDimPad);
}

QString bearingViewTitle(int index) {
    switch (index) {
        case 0: return QStringLiteral("主视图");
        case 1: return QStringLiteral("俯视图");
        case 2: return QStringLiteral("左视图");
    }
    return QStringLiteral("视图");
}

// ============================================================================
BearingViewsResult buildBearingViews(const BearingParams& p, const SketchModel* model) {
    BearingParams q = p; q.makeValid();
    const QRectF area = q.availableArea();
    const double gapH = std::max(q.gap, 8.0);
    const double gapV = gapH + 8.0;
    const double s = q.autoScale ? pickScale(q, area, gapH, gapV) : q.manualScale;

    BearingViewsResult R;
    R.scale = s; R.scaleLabel = scaleLabel(s); R.area = area;

    // ---- 模型量（mm）----
    const double bossRm = q.bossR(), ringRm = q.ringR(), boreRm = q.boreR();
    const double hl  = q.baseL * 0.5, hw = q.baseW * 0.5;   // 底板半长 / 半宽
    const double hbl = q.bossLen * 0.5;                     // 圆筒半长
    const double bossCyM = q.bossCy();                      // 圆筒轴心高
    const double webTopM = q.webTopY();                     // 支承板顶面高
    const double botHalfM = q.webBotW * 0.5;
    const double topHalfM = q.webTopHalf();
    const double zStepM   = q.zStep();
    const double slotCxM  = q.slotCx();
    const double slotHwM  = q.footSlotWd * 0.5;

    // ---- 图纸量（mm）----
    const double halfL = hl * s, halfW = hw * s;
    const double bossR = bossRm * s, ringR = ringRm * s, boreR = boreRm * s;
    const double bossCy = bossCyM * s;
    const double overallHs = q.overallH * s;
    const double extM = 4.0 / s;            // 中心线伸出量：图纸上 4mm

    // ---- 视图定位（第一角投影：俯视图在下、左视图在右）----
    const double blockW = 2.0 * halfL + gapH + 2.0 * halfW;
    const double blockH = overallHs + gapV + 2.0 * halfW;
    const double bx0 = area.x() + std::max(0.0, (area.width()  - blockW) * 0.5);
    const double by0 = area.y() + std::max(0.0, (area.height() - blockH) * 0.5);

    const double topX0 = bx0, topY0 = by0;                  // 俯视图（下）
    const double topCx = topX0 + halfL, topCy = topY0 + halfW;
    const double frontX0 = bx0, frontY0 = topY0 + 2.0 * halfW + gapV;   // 主视图（上）
    const double frontCx = frontX0 + halfL, frontCy = frontY0 + overallHs * 0.5;
    const double sideX0 = bx0 + 2.0 * halfL + gapH;         // 左视图（右）
    const double sideCx = sideX0 + halfW, sideCy = frontCy; // 高平齐

    R.frontCx = frontCx; R.frontCy = frontCy;
    R.topCx = topCx;     R.topCy   = topCy;
    R.sideCx = sideCx;   R.sideCy  = sideCy;
    R.bossR = bossR;     R.baseHalfL = halfL; R.baseHalfW = halfW;
    R.bounds = QRectF(bx0, by0, blockW, blockH);
    R.fits = R.bounds.left() >= area.x() - 1e-6 &&
             R.bounds.right() <= area.x() + area.width() + 1e-6 &&
             R.bounds.top() >= area.y() - 1e-6 &&
             R.bounds.bottom() <= area.y() + area.height() + 1e-6;

    // ---- 坐标映射（入参为模型 mm，出为图纸 mm，Y 轴向上）----
    // 主视图：x = 模型 X，y = 模型 Y
    auto F = [&](double x, double Y) { return Vec3(frontCx + x * s, frontY0 + Y * s, 0); };
    // 俯视图：x = 模型 X，y = −模型 Z（第一角：物体前方(+Z)朝图纸下方）
    auto T = [&](double x, double z) { return Vec3(topCx + x * s, topCy - z * s, 0); };
    // 左视图：x = 模型 Z，y = 模型 Y（第一角：物体前方(+Z)朝图纸右方）
    auto L = [&](double z, double Y) { return Vec3(sideCx + z * s, frontY0 + Y * s, 0); };

    // ================= 主视图（外形）=================
    {
        // 底板
        mkRect(R.elems, F(-hl, 0), F(hl, q.baseThk), QStringLiteral("主视图:底板"), model);
        // 地脚 U 形开口槽：沿 −Z 观察时被底板前后材料遮挡，槽底位置画细虚线
        for (int sgn : {-1, 1})
            R.elems.push_back(hidden(F(sgn * slotCxM, 0), F(sgn * slotCxM, q.baseThk),
                                     QStringLiteral("主视图:地脚槽底"), model));
        // 支承板两条斜边（顶面与轴承孔底相切，顶角落在圆筒外圆上）
        R.elems.push_back(solid(F(-botHalfM, q.baseThk), F(-topHalfM, webTopM),
                                QStringLiteral("主视图:支承板·左"), model));
        R.elems.push_back(solid(F( botHalfM, q.baseThk), F( topHalfM, webTopM),
                                QStringLiteral("主视图:支承板·右"), model));
        // 圆筒外圆：仅画支承板以外的可见弧（下半被支承板遮挡）
        if (topHalfM > 1e-6) {
            double a1 = std::atan2(webTopM - bossCyM,  topHalfM) * 180.0 / K_PI;
            double a2 = std::atan2(webTopM - bossCyM, -topHalfM) * 180.0 / K_PI;
            if (a2 < a1) a2 += 360.0;           // 逆时针扫过顶部
            R.elems.push_back(mkArc(F(0, bossCyM), bossR, a1, a2,
                                    QStringLiteral("主视图:圆筒外径"), model));
        }
        // 轴承孔 ØringD（可见）
        R.elems.push_back(mkCircle(F(0, bossCyM), ringR, LY_OUTLINE, LineStyle::Solid,
                                   C_OUTLINE, W_OUT, QStringLiteral("主视图:轴承孔"), model));
        // 台阶孔 ØboreD（穿过 ØringD 可见的台阶端面轮廓）
        R.elems.push_back(mkCircle(F(0, bossCyM), boreR, LY_OUTLINE, LineStyle::Solid,
                                   C_OUTLINE, W_OUT, QStringLiteral("主视图:台阶孔"), model));
        // 中心线
        R.elems.push_back(centerLine(F(-bossRm - extM, bossCyM), F(bossRm + extM, bossCyM),
                                     QStringLiteral("主视图:水平中心线"), model));
        R.elems.push_back(centerLine(F(0, -extM), F(0, q.overallH + extM),
                                     QStringLiteral("主视图:垂直中心线"), model));
    }

    // ================= 俯视图（外形）=================
    {
        // 底板：上下两条长边贯通；两端被 U 形开口槽切开 → 端边在槽口范围断开
        R.elems.push_back(solid(T(-hl, -hw), T(hl, -hw), QStringLiteral("俯视图:底板·下"), model));
        R.elems.push_back(solid(T(-hl,  hw), T(hl,  hw), QStringLiteral("俯视图:底板·上"), model));
        for (int sgn : {-1, 1}) {
            const double xe = sgn * hl, xc = sgn * slotCxM;
            for (int zs : {-1, 1}) {   // 端边（槽口以外）
                R.elems.push_back(solid(T(xe, zs * hw), T(xe, zs * slotHwM),
                                        QStringLiteral("俯视图:底板·端"), model));
                R.elems.push_back(solid(T(xe, zs * slotHwM), T(xc, zs * slotHwM),
                                        QStringLiteral("俯视图:地脚槽"), model));
            }
            // 槽底半圆：开口朝向底板端面
            const double a0 = (sgn > 0) ? 90.0 : -90.0;
            R.elems.push_back(mkArc(T(xc, 0), slotHwM * s, a0, a0 + 180.0,
                                    QStringLiteral("俯视图:地脚槽底"), model));
        }
        // 支承板外形（底宽 × 底板宽）
        mkRect(R.elems, T(-botHalfM, hw), T(botHalfM, -hw),
               QStringLiteral("俯视图:支承板"), model);
        // 支承板顶面边：仅圆筒外侧（|z| > bossLen/2）可见
        for (int sgn : {-1, 1})
            for (int zs : {-1, 1})
                R.elems.push_back(solid(T(sgn * topHalfM, zs * hbl), T(sgn * topHalfM, zs * hw),
                                        QStringLiteral("俯视图:支承板顶面"), model));
        // 圆筒外形（外径 × 轴向长）
        mkRect(R.elems, T(-bossRm, hbl), T(bossRm, -hbl),
               QStringLiteral("俯视图:圆筒"), model);
        // 轴承孔（被圆筒遮挡 → 虚线）
        for (int sgn : {-1, 1})
            R.elems.push_back(hidden(T(sgn * ringRm, zStepM), T(sgn * ringRm, hbl),
                                     QStringLiteral("俯视图:轴承孔"), model));
        // 台阶孔（虚线）
        for (int sgn : {-1, 1})
            R.elems.push_back(hidden(T(sgn * boreRm, -hbl), T(sgn * boreRm, zStepM),
                                     QStringLiteral("俯视图:台阶孔"), model));
        // 台阶端面（虚线）
        for (int sgn : {-1, 1})
            R.elems.push_back(hidden(T(sgn * ringRm, zStepM), T(sgn * boreRm, zStepM),
                                     QStringLiteral("俯视图:台阶端面"), model));
        // 中心线
        R.elems.push_back(centerLine(T(-hl - extM, 0), T(hl + extM, 0),
                                     QStringLiteral("俯视图:水平中心线"), model));
        R.elems.push_back(centerLine(T(0, hw + extM), T(0, -hw - extM),
                                     QStringLiteral("俯视图:垂直中心线"), model));
    }

    // ================= 左视图（全剖，画剖面线）=================
    {
        // 断面 1：底板 + 支承板 + 圆筒下壁（与后端台阶孔相连，合成一个多边形）
        const std::vector<Vec3> reg1 = {
            L(-hw,        0.0),
            L( hw,        0.0),
            L( hw,        webTopM),
            L( zStepM,    webTopM),
            L( zStepM,    bossCyM - boreRm),
            L(-hbl,       bossCyM - boreRm),
            L(-hbl,       webTopM),
            L(-hw,        webTopM),
        };
        // 断面 2：圆筒上壁（Ø96 段薄、Ø30 段厚）
        const std::vector<Vec3> reg2 = {
            L(-hbl,    bossCyM + bossRm),
            L( hbl,    bossCyM + bossRm),
            L( hbl,    bossCyM + ringRm),
            L( zStepM, bossCyM + ringRm),
            L( zStepM, bossCyM + boreRm),
            L(-hbl,    bossCyM + boreRm),
        };
        R.elems.push_back(makeBearingHatch(reg1, QStringLiteral("左视图:底板支承断面")));
        R.elems.push_back(makeBearingHatch(reg2, QStringLiteral("左视图:圆筒上壁断面")));

        // 中心线
        R.elems.push_back(centerLine(L(-hbl - extM, bossCyM), L(hbl + extM, bossCyM),
                                     QStringLiteral("左视图:轴线"), model));
        R.elems.push_back(centerLine(L(0, -extM), L(0, q.overallH + extM),
                                     QStringLiteral("左视图:垂直中心线"), model));
    }

    // ================= 尺寸标注（不含引线/引导线）=================
    const double dh = q.dimTextHeight;
    const double dOff = 8.0;

    // 主视图：总高（左侧）
    R.elems.push_back(mkDim(DimensionType::Vertical,
        F(-hl, 0), F(-hl, q.overallH), Vec3(frontX0 - dOff, frontCy, 0),
        n2(q.overallH), model, dh));
    // 主视图：Ø圆筒外径（尺寸线引到视图上方）
    R.elems.push_back(mkDim(DimensionType::Horizontal,
        F(-bossRm, bossCyM), F(bossRm, bossCyM),
        Vec3(frontCx, frontY0 + overallHs + dOff, 0), dimD(q.bossD), model, dh));
    // 俯视图：底板长（下方）
    R.elems.push_back(mkDim(DimensionType::Horizontal,
        T(-hl, hw), T(hl, hw), Vec3(topCx, topY0 - dOff, 0), n2(q.baseL), model, dh));
    // 俯视图：底板宽（右侧）
    R.elems.push_back(mkDim(DimensionType::Vertical,
        T(hl, hw), T(hl, -hw), Vec3(topX0 + 2.0 * halfL + dOff, topCy, 0),
        n2(q.baseW), model, dh));
    // 俯视图：圆筒轴向长（右侧，靠内）
    R.elems.push_back(mkDim(DimensionType::Vertical,
        T(bossRm, hbl), T(bossRm, -hbl), Vec3(topCx + bossR + dOff, topCy, 0),
        n2(q.bossLen), model, dh));
    // 俯视图：地脚槽宽（左侧槽，向底板外引出）
    R.elems.push_back(mkDim(DimensionType::Vertical,
        T(-slotCxM, slotHwM), T(-slotCxM, -slotHwM),
        Vec3(topCx - slotCxM * s - dOff, topCy, 0),
        n2(q.footSlotWd), model, dh));
    // 左视图：Ø轴承孔（右侧引出）
    {
        const double zc = zStepM + (hbl - zStepM) * 0.5;
        R.elems.push_back(mkDim(DimensionType::Vertical,
            L(zc, bossCyM - ringRm), L(zc, bossCyM + ringRm),
            Vec3(sideX0 + 2.0 * halfW + dOff, frontY0 + bossCy, 0),
            dimD(q.ringD), model, dh));
    }
    // 左视图：Ø台阶孔（左侧引出）
    {
        const double zc = -(hbl + zStepM) * 0.5;
        R.elems.push_back(mkDim(DimensionType::Vertical,
            L(zc, bossCyM - boreRm), L(zc, bossCyM + boreRm),
            Vec3(sideX0 - dOff, frontY0 + bossCy, 0),
            dimD(q.boreD), model, dh));
    }

    // 统计（不含引线）
    for (const auto& e : R.elems)
        if (e.type == ElementType::Dimension && e.dtype != DimensionType::Leader) ++R.dimCount;

    return R;
}

} // namespace SketchCAD
