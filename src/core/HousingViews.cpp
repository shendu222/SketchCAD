#include "HousingViews.h"
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
const QString LY_OUTLINE = QStringLiteral("箱体-轮廓");   // 粗实线：可见轮廓
const QString LY_HIDDEN  = QStringLiteral("箱体-虚线");   // 细虚线：不可见轮廓
const QString LY_CENTER  = QStringLiteral("箱体-中心线"); // 点划线
const QString LY_SECT    = QStringLiteral("箱体-剖面");   // 剖面线
const QString LY_DIM     = QStringLiteral("箱体-标注");   // 尺寸 / 符号
const QString LY_TEXT    = QStringLiteral("箱体-文字");   // 视图名 / 技术要求

const QColor C_OUTLINE(26, 29, 40);
const QColor C_HIDDEN (110, 120, 140);
const QColor C_CENTER (170, 60, 60);

constexpr double K_PI = 3.14159265358979323846;

// 主视图外扩半宽/半高（模型 mm）：顶部预留须覆盖粗糙度符号的图纸高度（≈2.6×字高，
// 与视图比例无关的纸面尺寸），故取 22（>9.1mm 符号高 + 余量）；底部留尺寸标注带 12。
constexpr double kMainHalfX = 68.0;
constexpr double kMainTopPad = 22.0, kMainBotPad = 12.0;
// 俯视图 / A—A / C 向外扩
constexpr double kTopHalf = 49.0;     // Ø82/2 + 中心线外扩 + 视图名
constexpr double kAaHalfX = 59.0, kAaHalfY = 55.0;
constexpr double kCHalf   = 27.0;
constexpr double kTechH   = 22.0;     // 技术要求文本块高

// ----------------------------- 图元构造辅助 -----------------------------
Element mkLine(const Vec3& a, const Vec3& b, const QString& layer,
               LineStyle ls, const QColor& col, double w,
               const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createLine(a, b, m);
    e.layer = layer; e.lineStyle = ls; e.color = col;
    e.lineWidthAuto = false; e.lineWidth = w;
    e.name = name;
    return e;
}
Element centerLine(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_CENTER, LineStyle::DashDot, C_CENTER, 0.5, name, m);
}
Element mkCircle(const Vec3& c, double r, double w, const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createCircle(c, r, m);
    e.layer = LY_OUTLINE; e.lineStyle = LineStyle::Solid; e.color = C_OUTLINE;
    e.lineWidthAuto = false; e.lineWidth = w; e.name = name;
    return e;
}
Element mkArc(const Vec3& c, double r, double a0, double a1, double w,
              const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createArc(c, r, a0, a1, m);
    e.layer = LY_OUTLINE; e.lineStyle = LineStyle::Solid; e.color = C_OUTLINE;
    e.lineWidthAuto = false; e.lineWidth = w; e.name = name;
    return e;
}
Element mkText(const Vec3& pos, const QString& t, double h, const QString& name,
               const SketchModel* m) {
    Element e = ElementFactory::createText(pos, t, h);
    (void)m;
    e.layer = LY_TEXT; e.color = C_OUTLINE; e.name = name;
    return e;
}
// 尺寸标注通过 SectionViewBuilder::addDim 统一生成，此处不再保留独立 mkDim。
// 引线标注：pts = {箭头尖, 拐点, 文本锚点}
Element mkLeader(const Vec3& tip, const Vec3& knee, const Vec3& anchor,
                 const QString& text, const SketchModel* m) {
    Element e = ElementFactory::createDimension(DimensionType::Leader, m);
    e.dtype = DimensionType::Leader; e.pts = {tip, knee, anchor};
    e.text = text; e.layer = LY_DIM; e.name = QStringLiteral("引线标注");
    e.dimTextHeight = 3.5;
    return e;
}

// 螺纹孔（俯视/端面方向）：小径整圆(粗实线) + 大径 3/4 圆弧(细实线) + 十字中心线
void mkThreadHole(std::vector<Element>& out, const Vec3& c, double s,
                  const QString& name, const SketchModel* m) {
    const double rMinor = 3.3 * 0.5 * s;      // M4 小径 ≈ Ø3.3
    const double rMajor = 4.0 * 0.5 * s;      // M4 大径 = Ø4.0
    out.push_back(mkCircle(c, rMinor, 2.0, name + QStringLiteral("·小径"), m));
    out.push_back(mkArc(c, rMajor, 45.0, 315.0, 0.6, name + QStringLiteral("·大径"), m));
    const double e1 = rMajor + 2.5 * s;
    out.push_back(centerLine(Vec3(c.x - e1, c.y, 0), Vec3(c.x + e1, c.y, 0), name, m));
    out.push_back(centerLine(Vec3(c.x, c.y - e1, 0), Vec3(c.x, c.y + e1, 0), name, m));
}

// 圆角矩形底板（四条直线 + 四个 R 角圆弧，粗实线）
void mkRoundRect(std::vector<Element>& out, double x0, double y0, double x1, double y1,
                 double r, const SketchModel* m) {
    const QString n = QStringLiteral("底板轮廓");
    out.push_back(mkLine(Vec3(x0 + r, y0, 0), Vec3(x1 - r, y0, 0), LY_OUTLINE,
                         LineStyle::Solid, C_OUTLINE, 2.0, n + QStringLiteral("·下"), m));
    out.push_back(mkLine(Vec3(x0 + r, y1, 0), Vec3(x1 - r, y1, 0), LY_OUTLINE,
                         LineStyle::Solid, C_OUTLINE, 2.0, n + QStringLiteral("·上"), m));
    out.push_back(mkLine(Vec3(x0, y0 + r, 0), Vec3(x0, y1 - r, 0), LY_OUTLINE,
                         LineStyle::Solid, C_OUTLINE, 2.0, n + QStringLiteral("·左"), m));
    out.push_back(mkLine(Vec3(x1, y0 + r, 0), Vec3(x1, y1 - r, 0), LY_OUTLINE,
                         LineStyle::Solid, C_OUTLINE, 2.0, n + QStringLiteral("·右"), m));
    out.push_back(mkArc(Vec3(x0 + r, y0 + r, 0), r, 180.0, 270.0, 2.0, n + QStringLiteral("·角1"), m));
    out.push_back(mkArc(Vec3(x1 - r, y0 + r, 0), r, 270.0, 360.0, 2.0, n + QStringLiteral("·角2"), m));
    out.push_back(mkArc(Vec3(x1 - r, y1 - r, 0), r, 0.0, 90.0, 2.0, n + QStringLiteral("·角3"), m));
    out.push_back(mkArc(Vec3(x0 + r, y1 - r, 0), r, 90.0, 180.0, 2.0, n + QStringLiteral("·角4"), m));
}

// ----------------------------- 数值格式化 -----------------------------
QString n2(double v) {
    const double r = std::round(v);
    if (std::fabs(v - r) < 1e-9) return QString::number((long long)r);
    return QString::number(v, 'f', 1);
}

const double kStdScales[] = {50.0, 20.0, 10.0, 5.0, 2.0, 1.0,
                             1.0 / 2.0, 1.0 / 2.5, 1.0 / 5.0, 1.0 / 10.0};
QString scaleLabel(double s) {
    if (std::fabs(s - 1.0) < 1e-9) return QStringLiteral("1:1");
    if (s > 1.0) return QStringLiteral("%1:1").arg(n2(s));
    return QStringLiteral("1:%1").arg(n2(1.0 / s));
}

// 视图总布置需求（模型 mm × s + 图纸常量 ≤ 可用区）
bool fitsSheet(double s, const QRectF& a, double gap, double overallH) {
    const double needW = 2.0 * (kMainHalfX + kCHalf) * s + 3.0 * gap;
    const double needH = (overallH + kMainTopPad + kMainBotPad + 2.0 * kAaHalfY + kTechH) * s
                         + 2.0 * gap;
    return needW <= a.width() + 1e-9 && needH <= a.height() + 1e-9;
}

} // namespace

// ============================================================================
// 参数合法性收敛
// ============================================================================
QString HousingParams::validate() const {
    if (outerD <= 0.0 || boreD <= 0.0) return QStringLiteral("筒径必须为正");
    if (boreD >= outerD) return QStringLiteral("主孔必须小于筒体外径（壁厚为正）");
    if (baseW <= 0.0 || baseL <= 0.0 || baseThk <= 0.0 || overallH <= 0.0)
        return QStringLiteral("底板尺寸必须为正");
    if (bossBoreD >= bossD) return QStringLiteral("凸台孔必须小于凸台外径");
    if (wallHoleCount < 0 || wallHoleCount > 36) return QStringLiteral("壁孔数量非法");
    if (wallHoleCount > 0) {
        if (wallHoleD <= 0.0) return QStringLiteral("壁孔径必须为正");
        if (wallHoleD >= wallThk() - 1.0)
            return QStringLiteral("壁孔径必须小于壁厚−1（孔完整落在壁内）");
    }
    if (bossAxisH - bossD * 0.5 < baseThk ||
        bossAxisH + bossD * 0.5 > overallH)
        return QStringLiteral("凸台必须完整落在筒身高度范围内");
    if (bossBoltCircle <= bossBoreD || bossBoltCircle + 2.0 >= bossD)
        return QStringLiteral("凸台螺孔分布圆必须介于凸台孔与外径之间");
    return QString();
}

void HousingParams::makeValid() {
    const auto clamp = [](double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); };
    outerD   = clamp(outerD, 10.0, 2000.0);
    boreD    = clamp(boreD, 2.0, outerD - 2.0);        // 不变式 1：壁厚 ≥1
    baseW    = clamp(baseW, 10.0, 2000.0);
    baseL    = clamp(baseL, 10.0, 2000.0);
    baseThk  = clamp(baseThk, 2.0, overallH * 0.5);
    overallH = clamp(overallH, baseThk + 10.0, 2000.0);
    cornerR  = clamp(cornerR, 0.0, std::min(baseW, baseL) * 0.25);
    bossD      = clamp(bossD, 6.0, outerD);
    bossBoreD  = clamp(bossBoreD, 2.0, bossD - 2.0);   // 不变式 2
    bossLen    = clamp(bossLen, 2.0, 200.0);
    bossAxisH  = clamp(bossAxisH, baseThk + bossD * 0.5 + 0.5,   // 不变式 3
                       overallH - bossD * 0.5 - 0.5);
    bossBoltCircle = clamp(bossBoltCircle, bossBoreD + 2.0, bossD - 4.0);
    wallHoleCount = std::max(0, std::min(36, wallHoleCount));
    if (wallHoleCount > 0)
        wallHoleD = clamp(wallHoleD, 1.0, std::max(1.0, wallThk() - 1.5));  // 不变式 2
}

double HousingParams::viewScale() const {
    HousingParams q = *this; q.makeValid();
    const QRectF a = q.availableArea();
    if (!autoScale) return manualScale;
    for (double cand : kStdScales)
        if (fitsSheet(cand, a, q.gap, q.overallH)) return cand;
    return kStdScales[sizeof(kStdScales) / sizeof(kStdScales[0]) - 1];
}

QRectF HousingParams::availableArea() const {
    const double x0 = marginL, x1 = sheetW - marginC;
    const double y0 = marginC, y1 = sheetH - marginC;
    QRectF full(x0, y0, std::max(20.0, x1 - x0), std::max(20.0, y1 - y0));
    const double tbH = std::min(titleBlockH, full.height() * 0.5);
    // 标题栏固定在右下角：绘图区取「标题栏上方整幅」与「标题栏左侧整幅」中面积较大者
    QRectF above(x0, y0 + tbH + gap, full.width(),
                 std::max(20.0, full.height() - (tbH + gap)));
    QRectF left(full.left(), y0, std::max(20.0, full.width() - (titleBlockW + gap)),
                full.height());
    return (above.width() * above.height() >= left.width() * left.height()) ? above : left;
}

QRectF HousingParams::viewsBounds() const {
    HousingParams q = *this; q.makeValid();
    const double s = q.viewScale();
    const double w = 2.0 * (kMainHalfX + kCHalf) * s + 3.0 * q.gap;
    const double h = (q.overallH + kMainTopPad + kMainBotPad + 2.0 * kAaHalfY + kTechH) * s
                     + 2.0 * q.gap;
    return QRectF(0, 0, w, h);
}

QString housingViewTitle(int index) {
    switch (index) {
        case 0: return QStringLiteral("主视图（全剖）");
        case 1: return QStringLiteral("俯视图 B");
        case 2: return QStringLiteral("A—A 断面");
        case 3: return QStringLiteral("C 向视图");
    }
    return QStringLiteral("视图");
}

// ============================================================================
// 视图生成
// ============================================================================
HousingViewsResult buildHousingViews(const HousingParams& p, const SketchModel* model) {
    HousingParams q = p;
    q.makeValid();
    const double s = q.viewScale();
    const QRectF area = q.availableArea();
    const double ax0 = area.left(), ay0 = area.top();          // 左 / 下
    const double ax1 = ax0 + area.width(), ay1 = ay0 + area.height(); // 右 / 上

    HousingViewsResult R;
    R.scale = s;
    R.scaleLabel = scaleLabel(s);
    R.area = area;

    const double oR = q.outerR(), bR = q.boreR();
    const double bossTip = oR + q.bossLen;                 // 凸台端面（模型 x）

    // ---- 锚点布置 ----
    // 主视图：左上（圆筒轴心 = 模型原点，fy 为底面标高）
    const double fy = ay1 - (q.overallH + kMainTopPad) * s;   // 主视图底面 y
    const double fx = ax0 + kMainHalfX * s + 2.0;
    // 俯视图 B：主视图正下方（长对正）
    const double by = fy - q.gap - kTopHalf * s;
    const double bx = fx;
    // A—A 断面：与俯视图同行、右侧
    const double aaCy = by;
    const double aaCx = ax0 + area.width() * 0.62;
    // C 向视图：右上，与凸台轴线平齐（高平齐）
    const double cCx = ax1 - kCHalf * s - 2.0;
    const double cCy = fy + q.bossAxisH * s;

    R.frontCx = fx; R.frontCy = fy;
    R.topCx = bx;   R.topCy = by;
    R.aaCx = aaCx;  R.aaCy = aaCy;
    R.cCx = cCx;    R.cCy = cCy;

    // ==================== 1. 主视图（全剖，SectionViewBuilder）====================
    {
        SectionViewBuilder sb(QStringLiteral("箱体"));
        SectionViewStyle st;
        st.hatchAngleDeg = 45.0;
        st.hatchSpacingMm = 3.0;
        st.textHeight = 3.5;
        sb.setStyle(st);

        const double fy0 = fy;                       // 模型 y=0（底面）在图纸上的 y
        auto Y = [&](double my) { return fy0 + my * s; };
        auto X = [&](double mx) { return fx + mx * s; };

        // 剖切到的实体区（模型坐标 → 图纸坐标；autoOutline 补粗实线轮廓）
        //   底板（实心矩形）
        sb.addCutRect(Vec3(X(-q.baseW/2), Y(0), 0), Vec3(X(q.baseW/2), Y(q.baseThk), 0), true);
        //   左筒壁
        sb.addCutRect(Vec3(X(-oR), Y(q.baseThk), 0), Vec3(X(-bR), Y(q.overallH), 0), true);
        //   右筒壁（被凸台轴向孔贯通，分成上下两段）
        const double hLo = q.bossAxisH - q.bossBoreD/2, hHi = q.bossAxisH + q.bossBoreD/2;
        sb.addCutRect(Vec3(X(bR), Y(q.baseThk), 0), Vec3(X(oR), Y(hLo), 0), true);
        sb.addCutRect(Vec3(X(bR), Y(hHi), 0), Vec3(X(oR), Y(q.overallH), 0), true);
        //   凸台上下壁（x: oR..bossTip，壁厚 4 = (bossD-bossBoreD)/2）
        const double bt = (q.bossD - q.bossBoreD) * 0.5;
        sb.addCutRect(Vec3(X(oR), Y(hHi), 0), Vec3(X(bossTip), Y(hHi + bt), 0), true);
        sb.addCutRect(Vec3(X(oR), Y(hLo - bt), 0), Vec3(X(bossTip), Y(hLo), 0), true);

        // 中心线：竖直轴线 + 凸台轴线
        sb.addCenterLine(Vec3(X(0), Y(-4.0), 0), Vec3(X(0), Y(q.overallH + 4.0), 0));
        sb.addCenterLine(Vec3(X(-6.0), Y(q.bossAxisH), 0), Vec3(X(bossTip + 6.0), Y(q.bossAxisH), 0));

        // 尺寸标注
        sb.addDim(DimensionType::Vertical, Vec3(X(oR), Y(0), 0), Vec3(X(oR), Y(q.overallH), 0),
                  Vec3(X(oR + 14.0), Y(q.overallH * 0.5), 0), n2(q.overallH));              // 总高
        sb.addDim(DimensionType::Vertical, Vec3(X(bossTip), Y(0), 0), Vec3(X(bossTip), Y(q.bossAxisH), 0),
                  Vec3(X(bossTip + 26.0), Y(q.bossAxisH * 0.5), 0), n2(q.bossAxisH));       // 凸台轴高
        sb.addDim(DimensionType::Vertical, Vec3(X(-oR), Y(q.baseThk), 0), Vec3(X(-oR), Y(q.overallH), 0),
                  Vec3(X(-oR - 12.0), Y((q.baseThk + q.overallH) / 2), 0),
                  QStringLiteral("⌀") + n2(q.outerD));                                      // ⌀82
        sb.addDim(DimensionType::Vertical, Vec3(X(-bR), Y(q.baseThk), 0), Vec3(X(-bR), Y(q.overallH), 0),
                  Vec3(X(-bR - 12.0), Y((q.baseThk + q.overallH) / 2), 0),
                  QStringLiteral("⌀") + n2(q.boreD) + (q.boreTol.isEmpty() ? QString() : q.boreTol)); // ⌀64H8
        sb.addDim(DimensionType::Vertical, Vec3(X(-q.baseW/2), Y(0), 0), Vec3(X(-q.baseW/2), Y(q.baseThk), 0),
                  Vec3(X(-q.baseW/2 - 8.0), Y(q.baseThk * 0.5), 0), n2(q.baseThk));        // 7
        sb.addDim(DimensionType::Horizontal, Vec3(X(-q.baseW/2), Y(0), 0), Vec3(X(q.baseW/2), Y(0), 0),
                  Vec3(X(0), Y(-10.0), 0), n2(q.baseW));                                   // 46

        // 粗糙度符号：腔底 Ra 1.6、筒顶环形面 Ra 6.3
        {
            RoughnessSpec rs;
            rs.anchor = Vec3(X(8.0), Y(q.baseThk), 0);
            rs.textHeight = 3.5;
            rs.value = QStringLiteral("Ra 1.6");
            sb.addRoughness(rs);
            RoughnessSpec rs2;
            rs2.anchor = Vec3(X((bR + oR) / 2), Y(q.overallH), 0);
            rs2.textHeight = 3.5;
            rs2.value = QStringLiteral("Ra 6.3");
            sb.addRoughness(rs2);
        }

        const SectionViewResult svr = sb.build();
        for (auto& e : svr.elems) R.elems.push_back(std::move(e));
        R.hatchCount += svr.hatchCount;
        R.dimCount   += svr.dimCount;
        R.roughnessCount += 2;

        // 剖切符号 A-A（水平剖切面，两端位置线 + 向下箭头 + 字母 A）
        const double cutY = Y(q.overallH - 10.0);
        auto cut = buildCutAnnotation(Vec3(X(-oR - 4.0), cutY, 0), Vec3(X(oR + 4.0), cutY, 0),
                                      CutViewDir::Down, QStringLiteral("A"), 5.0, LY_DIM);
        R.cutSymbolCount = (int)cut.size();
        for (auto& e : cut) R.elems.push_back(std::move(e));
    }

    // ==================== 2. 俯视图 B（底板 + 螺孔 + 筒圆）====================
    {
        const double x0 = bx - (q.baseW/2) * s, x1 = bx + (q.baseW/2) * s;
        const double y0 = by - (q.baseL/2) * s, y1 = by + (q.baseL/2) * s;
        const double rr = std::min(q.cornerR * s, std::min(x1 - x0, y1 - y0) * 0.25);
        if (rr > 0.1) mkRoundRect(R.elems, x0, y0, x1, y1, rr, model);
        else {
            R.elems.push_back(mkLine(Vec3(x0, y0, 0), Vec3(x1, y0, 0), LY_OUTLINE, LineStyle::Solid,
                                     C_OUTLINE, 2.0, QStringLiteral("底板轮廓·下"), model));
            R.elems.push_back(mkLine(Vec3(x1, y0, 0), Vec3(x1, y1, 0), LY_OUTLINE, LineStyle::Solid,
                                     C_OUTLINE, 2.0, QStringLiteral("底板轮廓·右"), model));
            R.elems.push_back(mkLine(Vec3(x1, y1, 0), Vec3(x0, y1, 0), LY_OUTLINE, LineStyle::Solid,
                                     C_OUTLINE, 2.0, QStringLiteral("底板轮廓·上"), model));
            R.elems.push_back(mkLine(Vec3(x0, y1, 0), Vec3(x0, y0, 0), LY_OUTLINE, LineStyle::Solid,
                                     C_OUTLINE, 2.0, QStringLiteral("底板轮廓·左"), model));
        }

        // 四角 4×M4-7H
        const double hx = (q.baseW/2 - 6.0) * s, hy = (q.baseL/2 - 6.0) * s;
        Vec3 firstHole;
        for (int i = 0; i < 4; ++i) {
            const Vec3 c(bx + (i % 2 == 0 ? -hx : hx), by + (i < 2 ? hy : -hy), 0);
            if (i == 0) firstHole = c;
            mkThreadHole(R.elems, c, s, QStringLiteral("底板螺孔"), model);
        }

        // 圆筒外圆（可见，悬出底板）+ 主孔（顶部开口，可见）
        R.elems.push_back(mkCircle(Vec3(bx, by, 0), oR * s, 2.0, QStringLiteral("筒体外圆"), model));
        R.elems.push_back(mkCircle(Vec3(bx, by, 0), bR * s, 2.0, QStringLiteral("主孔"), model));
        // 底板被筒体遮住的边（虚线）
        R.elems.push_back(mkLine(Vec3(x0, by, 0), Vec3(bx - oR * s, by, 0), LY_HIDDEN,
                                 LineStyle::Dashed, C_HIDDEN, 1.0, QStringLiteral("底板边·虚"), model));
        R.elems.push_back(mkLine(Vec3(bx + oR * s, by, 0), Vec3(x1, by, 0), LY_HIDDEN,
                                 LineStyle::Dashed, C_HIDDEN, 1.0, QStringLiteral("底板边·虚"), model));
        // 中心线
        const double ce = oR * s + 5.0;
        R.elems.push_back(centerLine(Vec3(bx - ce, by, 0), Vec3(bx + ce, by, 0),
                                     QStringLiteral("俯视中心线"), model));
        R.elems.push_back(centerLine(Vec3(bx, by - ce, 0), Vec3(bx, by + ce, 0),
                                     QStringLiteral("俯视中心线"), model));
        // 视图名 B + 螺孔规格引线
        R.elems.push_back(mkText(Vec3(bx, y1 + 5.0, 0), QStringLiteral("B"), 5.0,
                                 QStringLiteral("视图名"), model));
        R.elems.push_back(mkLeader(Vec3(firstHole.x + 3.3 * 0.5 * s * 0.7071,
                                        firstHole.y + 3.3 * 0.5 * s * 0.7071, 0),
                                   Vec3(x0 - 6.0, y1 + 2.0, 0), Vec3(x0 - 6.0, y1 + 2.0, 0),
                                   q.mountHoleSpec, model));
    }

    // ==================== 3. A—A 断面图（环形断面 + 3×Ø7 壁孔）====================
    {
        RingSectionSpec rs;
        rs.center = Vec3(aaCx, aaCy, 0);
        rs.outerD = q.outerD;
        rs.innerD = q.boreD;
        for (int i = 0; i < q.wallHoleCount; ++i) {
            RingSectionSpec::WallHole wh;
            // 壁孔组绕下半圆布置（其中一组元正对 270°，与附图 A—A 姿态一致）
            wh.angleDeg = 270.0 + (i - (q.wallHoleCount - 1) / 2.0)
                                * (360.0 / std::max(1, q.wallHoleCount));
            wh.d = q.wallHoleD;
            rs.wallHoles.push_back(wh);
        }
        rs.hatchAngleDeg = 45.0;
        rs.hatchSpacingMm = 3.0;
        rs.textHeight = 3.5;
        rs.layerPrefix = QStringLiteral("箱体");

        SectionViewResult rr2;
        auto ringElems = makeRingSection(rs, &rr2);
        for (auto& e : ringElems) R.elems.push_back(std::move(e));
        R.hatchCount += rr2.hatchCount;
        R.dimCount   += rr2.dimCount;

        // 壁孔规格引线（3×Ø7，指向最下方壁孔）
        if (q.wallHoleCount > 0) {
            const double rMid = (oR + bR) * 0.5;
            const double a = 270.0 * K_PI / 180.0;
            const Vec3 tip(aaCx + rMid * s * std::cos(a) + q.wallHoleD * 0.5 * s * 0.7071,
                           aaCy + rMid * s * std::sin(a) + q.wallHoleD * 0.5 * s * 0.7071, 0);
            R.elems.push_back(mkLeader(tip,
                                       Vec3(aaCx + rMid * s + 10.0, aaCy - rMid * s - 6.0, 0),
                                       Vec3(aaCx + rMid * s + 10.0, aaCy - rMid * s - 6.0, 0),
                                       QStringLiteral("%1×Ø%2").arg(q.wallHoleCount).arg(n2(q.wallHoleD)),
                                       model));
        }
        // 断面图名 A—A（视图上方居中）
        R.elems.push_back(buildSectionViewLabel(Vec3(aaCx, aaCy + oR * s + 10.0, 0),
                                                QStringLiteral("A"), 5.0, LY_TEXT));
    }

    // ==================== 4. C 向视图（凸台端面）====================
    {
        R.elems.push_back(mkCircle(Vec3(cCx, cCy, 0), q.bossD * 0.5 * s, 2.0,
                                   QStringLiteral("凸台外圆"), model));
        R.elems.push_back(mkCircle(Vec3(cCx, cCy, 0), q.bossBoreD * 0.5 * s, 2.0,
                                   QStringLiteral("凸台孔"), model));
        // 4×M4-7H（分布圆上 45° 姿态）
        const double bc = q.bossBoltCircle * 0.5 * s;
        Vec3 firstHole;
        for (int i = 0; i < 4; ++i) {
            const double a = (45.0 + i * 90.0) * K_PI / 180.0;
            const Vec3 c(cCx + bc * std::cos(a), cCy + bc * std::sin(a), 0);
            if (i == 0) firstHole = c;
            mkThreadHole(R.elems, c, s, QStringLiteral("凸台螺孔"), model);
        }
        // 中心线
        const double ce = q.bossD * 0.5 * s + 5.0;
        R.elems.push_back(centerLine(Vec3(cCx - ce, cCy, 0), Vec3(cCx + ce, cCy, 0),
                                     QStringLiteral("C向中心线"), model));
        R.elems.push_back(centerLine(Vec3(cCx, cCy - ce, 0), Vec3(cCx, cCy + ce, 0),
                                     QStringLiteral("C向中心线"), model));
        // 视图名 C + 螺孔规格引线
        R.elems.push_back(mkText(Vec3(cCx, cCy - q.bossD * 0.5 * s - 8.0, 0),
                                 QStringLiteral("C"), 5.0, QStringLiteral("视图名"), model));
        R.elems.push_back(mkLeader(Vec3(firstHole.x + 4.0 * 0.5 * s * 0.7071,
                                        firstHole.y + 4.0 * 0.5 * s * 0.7071, 0),
                                   Vec3(cCx - q.bossD * 0.5 * s - 4.0, cCy + q.bossD * 0.5 * s + 8.0, 0),
                                   Vec3(cCx - q.bossD * 0.5 * s - 4.0, cCy + q.bossD * 0.5 * s + 8.0, 0),
                                   q.bossHoleSpec, model));
    }

    // ==================== 5. 技术要求 + 其余粗糙度 ====================
    {
        const double tx = aaCx - 55.0;
        const double ty = ay0 + kTechH * s;
        R.elems.push_back(mkText(Vec3(tx, ty, 0), QStringLiteral("技术要求"), 5.0,
                                 QStringLiteral("技术要求标题"), model));
        R.elems.push_back(mkText(Vec3(tx, ty - 7.0, 0), QStringLiteral("1. 未注圆角 R3。"), 3.5,
                                 QStringLiteral("技术要求"), model));
        R.elems.push_back(mkText(Vec3(tx, ty - 13.0, 0), QStringLiteral("2. 未注倒角 C1。"), 3.5,
                                 QStringLiteral("技术要求"), model));
    }
    {
        // 「其余」粗糙度：图纸右上角（图框内侧）
        RoughnessSpec rs;
        rs.anchor = Vec3(q.sheetW - q.marginC - 8.0, q.sheetH - q.marginC - 12.0, 0);
        rs.textHeight = 3.5;
        rs.value = QStringLiteral("其余");
        rs.layer = LY_DIM;
        auto syms = buildRoughnessSymbol(rs);
        for (auto& e : syms) R.elems.push_back(std::move(e));
        R.roughnessCount += 1;
    }

    // ==================== 包围盒与放得下判定 ====================
    {
        QRectF b; bool has = false;
        for (const auto& e : R.elems) {
            const QRectF eb = geo::elementBounds(e);
            b = has ? b.united(eb) : eb; has = true;
        }
        R.bounds = b;
    }
    R.fits = area.contains(R.bounds) || (R.bounds.left() >= ax0 - 0.5 &&
             R.bounds.right() <= ax1 + 0.5 && R.bounds.top() >= ay0 - 0.5 &&
             R.bounds.bottom() <= ay1 + 0.5);
    return R;
}

} // namespace SketchCAD
