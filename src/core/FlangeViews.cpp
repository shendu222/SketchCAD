#include "FlangeViews.h"
#include "ElementFactory.h"
#include "Geometry.h"        // geo::elementBounds（视图包围盒与"是否放得下"的判据）
#include "SketchModel.h"
#include "DimStyle.h"
#include <QColor>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace SketchCAD {
namespace {

// ----------------------------- 图层与线型约定 -----------------------------
// 与 sheet_frame.cpp 的「图框 / 标题栏」图层互不冲突，便于在 BOM 树里分别控制显示。
const QString LY_OUTLINE = QStringLiteral("法兰-轮廓");   // 粗实线：可见轮廓
const QString LY_HIDDEN  = QStringLiteral("法兰-虚线");   // 细虚线：不可见轮廓
const QString LY_CENTER  = QStringLiteral("法兰-中心线"); // 点划线：轴线与中心圆
const QString LY_DIM     = QStringLiteral("法兰-标注");   // 尺寸标注
const QString LY_TEXT    = QStringLiteral("法兰-文字");   // 视图名 / 说明

const QColor C_OUTLINE(26, 29, 40);    // 与默认前景色一致（深灰）
const QColor C_HIDDEN (110, 120, 140); // 虚线用浅灰
const QColor C_CENTER (170, 60, 60);   // 中心线用暗红（与 AircraftViews 同惯例）

constexpr double K_PI = 3.14159265358979323846;

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

Element solid(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, 2.0, name, m);
}
Element hidden(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_HIDDEN, LineStyle::Dashed, C_HIDDEN, 1.0, name, m);
}
Element centerLine(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* m) {
    return mkLine(a, b, LY_CENTER, LineStyle::DashDot, C_CENTER, 0.5, name, m);
}

Element mkCircle(const Vec3& c, double r, const QString& layer,
                 LineStyle ls, const QColor& col, double w,
                 const QString& name, const SketchModel* m) {
    Element e = ElementFactory::createCircle(c, r, m);
    e.layer = layer; e.lineStyle = ls; e.color = col;
    e.lineWidthAuto = false; e.lineWidth = w;
    e.name = name;
    return e;
}

// 圆弧的起止角一律用 **标准数学角**（0° = +X，逆时针为正，单位度）。
// 这正是 circle/arc 图元在模型里的存储约定（见 geo::sampleArc / Element.startAngle），
// 也是本模块所有三角计算采用的约定 —— 二者一致，不做任何角度翻转。

Element mkText(const Vec3& pos, const QString& t, double h, bool centered) {
    Element e = ElementFactory::createText(pos, t, h);
    e.layer = LY_TEXT;
    e.color = C_OUTLINE;
    e.name = centered ? QStringLiteral("视图名") : QStringLiteral("说明文字");
    return e;
}

// 尺寸标注：pts = {测量点A, 测量点B, 尺寸线位置}
Element mkDim(DimensionType dt, const Vec3& a, const Vec3& b, const Vec3& pos,
              const QString& text, const SketchModel* m, double dimTextH = 0.0) {
    Element e = ElementFactory::createDimension(dt, m);
    e.dtype = dt;
    e.pts = {a, b, pos};
    e.text = text;
    e.layer = LY_DIM;
    e.name = QStringLiteral("尺寸标注");
    e.dimTextHeight = dimTextH;
    return e;
}

// ----------------------------- 数值格式化 -----------------------------
// 整数不拖 ".00"，非整数保留一位（法兰尺寸表里 C=3.2 / r=1.6 这类值必须保住小数）。
QString n2(double v) {
    const double r = std::round(v);
    if (std::fabs(v - r) < 1e-9) return QString::number((long long)r);
    return QString::number(v, 'f', 1);
}
QString mmPrefix() { return QStringLiteral("⌀"); }
QString dimD(double v) { return mmPrefix() + n2(v); }

// ----------------------------- 标准比例表 -----------------------------
// GB/T 14690 优先推荐的比例：缩小 1:2、1:2.5、1:5 …；放大 2:1、5:1 …
// 只列真实存在的标准比例，绝不使用 1:1.7 这种非标值。
const double kStdScales[] = {50.0, 20.0, 10.0, 5.0, 2.0, 1.0,
                             1.0 / 2.0, 1.0 / 2.5, 1.0 / 5.0, 1.0 / 10.0,
                             1.0 / 20.0, 1.0 / 50.0, 1.0 / 100.0};

// 把比例因子写成标准比例串：2.0 → "2:1"；0.5 → "1:2"；0.4 → "1:2.5"
QString scaleLabel(double s) {
    if (std::fabs(s - 1.0) < 1e-9) return QStringLiteral("1:1");
    if (s > 1.0) return QStringLiteral("%1:1").arg(n2(s));
    return QStringLiteral("1:%1").arg(n2(1.0 / s));
}

} // namespace

// ============================================================================
// 参数合法性收敛
// ============================================================================
QString FlangeParams::validate() const {
    if (outerD <= 0.0)      return QStringLiteral("外径必须为正");
    if (thickness <= 0.0)   return QStringLiteral("厚度必须为正");
    if (boltHoleD <= 0.0)   return QStringLiteral("螺栓孔径必须为正");
    if (boltCount < 1)      return QStringLiteral("螺栓孔数量至少为 1");
    if (boltCount > 64)     return QStringLiteral("螺栓孔数量过多（上限 64）");
    if (hubD <= 0.0)        return QStringLiteral("颈部外径必须为正");
    if (hubHeight <= 0.0)   return QStringLiteral("颈部总高必须为正");
    // 不变式 1：带颈模式下凸台必须从盘面凸出（否则退化成无颈平板，与附图结构不符）
    if (hasHub && hubHeight <= thickness + 1e-9)
        return QStringLiteral("颈部总高必须大于法兰厚度（否则凸台消失）");
    // 不变式 2：内孔必须明显小于盘体
    if (hubD > outerD)
        return QStringLiteral("颈部外径不能大于法兰外径");
    if (boreD <= 0.0)       return QStringLiteral("中心孔径必须为正");
    if (boreD >= hubD)      return QStringLiteral("中心孔径必须小于颈部外径（颈部壁厚为负）");
    // 不变式 4：止口（凹槽）必须包住内孔且不穿出外缘
    if (recessDepth > 0.0) {
        if (recessD <= 0.0)        return QStringLiteral("止口深度>0 时止口外径必须为正");
        if (recessD >= outerD)     return QStringLiteral("止口外径不能大于法兰外径");
        if (recessD <= boreD)      return QStringLiteral("止口外径必须大于中心孔径（止口包不住内孔）");
        if (recessDepth >= thickness) return QStringLiteral("止口深度必须小于法兰厚度");
    }
    // 不变式 3：螺栓孔完全落在盘体的环形实体区内（内孔外缘 … 外缘，各留 0.5mm 余量）
    //
    // 注意下限取**中心孔**而不是颈部外径：四孔法兰的螺栓孔常落在颈部外圆以内
    // （孔只穿过盘体，与颈部并不干涉 —— 附表 DN10~65 各档正是如此）。
    // 若用 hubD/2 当下限，会把这种合法布局误判为非法并强行外推，
    // 那样不仅改了用户给定的 K，也偏离了标准尺寸表。
    const double rBoltC = boltCircle * 0.5;
    const double rBolt  = boltHoleD * 0.5;
    const double rIn     = boreD * 0.5 + 0.5;          // 中心孔外缘 + 余量
    const double rOutLim = outerD * 0.5 - 0.5;         // 盘体外缘 − 余量
    if (rBoltC - rBolt < rIn - 1e-9)
        return QStringLiteral("螺栓孔内缘切进中心孔");
    if (rBoltC + rBolt > rOutLim + 1e-9)
        return QStringLiteral("螺栓孔外缘切出法兰外缘");
    return QString();
}

void FlangeParams::makeValid() {
    const auto clamp = [](double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); };

    outerD     = clamp(outerD, 20.0, 5000.0);
    thickness  = clamp(thickness, 1.0, outerD * 0.5);
    boltHoleD  = clamp(boltHoleD, 1.0, outerD * 0.2);
    boltCount  = std::max(1, std::min(64, boltCount));

    // 形态切换：带颈 → 凸台必须凸出；平板 → 轴向总长=厚度，颈部外径无意义（对齐外圆）
    if (hasHub) {
        hubD      = clamp(hubD, 5.0, outerD);
        // 不变式 1：颈部总高至少比厚度大 10%（保证凸台可见）
        hubHeight = std::max(hubHeight, thickness * 1.10 + 0.5);
    } else {
        hubD      = outerD;
        hubHeight = thickness;          // 平板：轴向总长 = 盘体厚度
    }

    boreD      = clamp(boreD, 1.0, hubD - 1.0);
    hubChamfer = clamp(hubChamfer, 0.0, std::min(3.0, hubHeight * 0.2));
    boreChamfer= clamp(boreChamfer, 0.0, std::min(4.0, thickness * 0.3));

    // 不变式 4：止口（凹槽）收敛 —— 外径夹进 (bore, outerD) 环带，深度不超过厚度 85%
    if (recessDepth > 0.0) {
        recessD    = clamp(recessD, boreD + 2.0, outerD - 2.0);
        recessDepth= std::min(recessDepth, std::min(thickness, hubHeight) * 0.85);
        if (recessDepth <= 0.0) recessDepth = 0.0;
    } else {
        recessD = 0.0;
    }

    // 不变式 3：螺栓孔分布圆收进合法环带（下限用中心孔口径，理由见 validate()）
    const double rOut   = outerD * 0.5;
    const double rBolt  = boltHoleD * 0.5;
    const double rInner = boreD * 0.5 + rBolt + 0.5;    // 内缘下限（含 0.5mm 余量）
    const double rOuter = rOut - rBolt - 0.5;           // 外缘上限
    double rBoltC = boltCircle * 0.5;
    if (rOuter <= rInner) {
        // 环带太窄放不下一圈孔：先压小孔径，再把分布圆放到环带中点
        boltHoleD = clamp((rOut - boreD * 0.5) * 0.5, 1.0, boltHoleD);
        const double rB2 = boltHoleD * 0.5;
        const double lo = boreD * 0.5 + rB2 + 0.25, hi = rOut - rB2 - 0.25;
        rBoltC = (hi > lo) ? (lo + hi) * 0.5 : std::max(0.5, hi);
    } else {
        rBoltC = clamp(rBoltC, rInner, rOuter);         // 合法区间内原样保留用户给的 K
    }
    boltCircle = rBoltC * 2.0;
}

// ============================================================================
// 视图尺寸与比例计算
// ============================================================================
namespace {

// 三视图按用户指定布局（主视图与侧视图并排位于上方、俯视图置于主视图正下方）：
//   ┌──────────┬──────────┐
//   │ 主视图   │ 侧视图   │   ← 上行：主视图(圆) 与 侧视图(矩形) 并排，"高平齐"（Y 范围一致）
//   │ （圆）   │ （矩形） │
//   ├──────────┴──────────┤
//   │   俯视图（矩形，位于主视图正下方，"长对正"）   │
//   └─────────────────────┘
//   主视图为圆 ⇒ 侧视图、俯视图均为矩形（仅球体三视图可均为圆）。
//   注：本布局把俯视图放在主视图正下方、侧视图放在主视图右侧，即为 GB/T 17451 规定的
//       「第一角画法」标准布局（俯视图在下、左视图在右）；三视图对齐关系严格满足
//       长对正 / 高平齐 / 宽相等。
struct LayoutPlan {
    double scale = 1.0;
    double Vw = 0, Hv = 0;         // 侧视图宽(轴向) / 高(直径) == 俯视图高(轴向) / 宽(直径)
    double frontR = 0;             // 主视图外圆半径
    double profHalfA = 0;          // 侧/俯视图轴向半长（= hubHeight*scale/2）
    double profHalfD = 0;          // 侧/俯视图直径半高（= outerD*scale/2）
    double gapH = 0, gapV = 0, titleMargin = 0;
    double totalW = 0, totalH = 0;
    bool   hitMin = false;         // 是否触到最小比例（空间仍不够）
};

// 图名占位（图纸量，与比例无关）：4mm 字高 + 余量
constexpr double kTitleMargin = 7.0;   // 上下各留 7mm 放图名 + 标注余量

LayoutPlan planLayout(const FlangeParams& p, double availW, double availH,
                      double gapH, double gapV) {
    LayoutPlan L;
    // 模型在纸上的实际占用：
    //   宽 = (D + hubHeight) * s + gapH
    //   高 = (D + hubHeight) * s + gapV
    // 图名与最外侧标注再各留 titleMargin（上下/左右都留，故减 2×titleMargin 作为安全余量）。
    const double kModel = p.outerD + p.hubHeight;
    const double safeW = std::max(0.0, availW - 2.0 * kTitleMargin);
    const double safeH = std::max(0.0, availH - 2.0 * kTitleMargin);

    // 从最大比例开始试，取第一个放得下的标准比例。
    double s = kStdScales[sizeof(kStdScales) / sizeof(kStdScales[0]) - 1];
    for (double cand : kStdScales) {
        const double totalW = kModel * cand + gapH;
        const double totalH = kModel * cand + gapV;
        if (totalW <= safeW + 1e-9 && totalH <= safeH + 1e-9) { s = cand; break; }
    }
    const double finalW = kModel * s + gapH;
    const double finalH = kModel * s + gapV;
    if (finalW > safeW + 1e-9 || finalH > safeH + 1e-9) L.hitMin = true;

    L.scale      = s;
    L.frontR     = p.outerD  * 0.5 * s;
    L.profHalfA  = p.hubHeight * 0.5 * s;
    L.profHalfD  = p.outerD  * 0.5 * s;
    L.Vw         = p.hubHeight * s;
    L.Hv         = p.outerD  * s;
    L.gapH = gapH; L.gapV = gapV; L.titleMargin = kTitleMargin;
    L.totalW = finalW; L.totalH = finalH;
    return L;
}

} // namespace

double FlangeParams::viewScale() const {
    FlangeParams q = *this; q.makeValid();
    const QRectF a = q.availableArea();
    const double gapH = std::max(gap, 6.0);
    const double gapV = gapH + 10.0;
    const LayoutPlan L = planLayout(q, a.width(), a.height(), gapH, gapV);
    return autoScale ? L.scale : manualScale;
}

QRectF FlangeParams::availableArea() const {
    // 可用区 = 图框内减掉标题栏占用（含安全间隙）
    const double a = marginL, c = marginC;
    QRectF r(a, c, sheetW - c - a, sheetH - c - c);
    const double tbH = std::min(titleBlockH, r.height() * 0.5);
    const double tbW = std::min(titleBlockW, r.width() * 0.7);
    // 标题栏贴在右下角：把 r 的底边抬到标题栏顶 + 间隙
    r.setBottom(std::max(r.top() + 20.0, r.bottom() - (tbH + gap)));
    // 标题栏向左伸出的部分也不能被视图压到：把 r 的右边收进 tbW + 间隙
    r.setRight(std::max(r.left() + 20.0, r.right() - (tbW + gap)));
    return r;
}

// ============================================================================
// 三面图生成
// ============================================================================
namespace {

// ---- 主视图（正对法兰盘面看）：同心圆 + 螺栓孔 + 中心线 ----
// 圆心 (cx, cy) 位于图纸坐标；所有半径已乘 scale。
// 线型选择依据「投影后是否可见」：
//   外圆、螺栓孔      → 通孔/外缘，可见 → 粗实线
//   中心孔            → 通孔内壁，可见 → 粗实线
//   颈部外圆          → 凸台在背面并被盘面遮挡，按正投影可见性不画（主视图正是圆盘面）。
void buildFrontView(std::vector<Element>& out, const FlangeParams& p, double s,
                    double cx, double cy, const SketchModel* m) {
    const double rOut  = p.outerD    * 0.5 * s;
    const double rBore = p.boreD     * 0.5 * s;
    const double rBoltC= p.boltCircle* 0.5 * s;
    const double rBolt = p.boltHoleD * 0.5 * s;
    const Vec3  c(cx, cy, 0);

    out.push_back(mkCircle(c, rOut, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, 2.0,
                           QStringLiteral("主视图:法兰外圆"), m));
    // 主视图正对法兰盘面，颈部外圆在背面并被盘面遮挡，按正投影不画。
    out.push_back(mkCircle(c, rBore, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, 2.0,
                           QStringLiteral("主视图:中心孔"), m));

    // 中心孔倒角圆（细实线，凸台端面的倒角棱边）
    if (p.boreChamfer > 1e-6)
        out.push_back(mkCircle(c, std::max(1e-6, rBore + p.boreChamfer * s),
                               LY_OUTLINE, LineStyle::Solid, C_HIDDEN, 1.0,
                               QStringLiteral("主视图:中心孔倒角"), m));

    // 止口（凹槽）外圆：止口是端面上的真实圆形棱边，正投影可见 → 粗实线
    //   （仅当 recessD>0 且 recessDepth>0；平板止口法兰即在主视图呈现"外圆/止口/内孔"三层同心圆）
    if (p.recessDepth > 1e-6 && p.recessD > 1e-6) {
        const double rRecess = p.recessD * 0.5 * s;
        out.push_back(mkCircle(c, rRecess, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, 2.0,
                               QStringLiteral("主视图:止口外圆"), m));
    }

    // 螺栓孔分布圆（点划线）——即题目所称"螺栓孔分布圆"，必须画出来才好标注 K
    out.push_back(mkCircle(c, rBoltC, LY_CENTER, LineStyle::DashDot, C_CENTER, 0.5,
                           QStringLiteral("主视图:螺栓孔分布圆"), m));

    // 螺栓孔：以 p.boltAngle0Deg 为起始角，沿标准数学角（0°=+X，逆时针）均匀分布。
    for (int i = 0; i < p.boltCount; ++i) {
        const double a = p.boltAngle0Deg + 360.0 * i / p.boltCount;
        const double rad = a * K_PI / 180.0;
        out.push_back(mkCircle(Vec3(cx + rBoltC * std::cos(rad),
                                    cy + rBoltC * std::sin(rad), 0),
                               rBolt, LY_OUTLINE, LineStyle::Solid, C_OUTLINE, 2.0,
                               QStringLiteral("主视图:螺栓孔%1").arg(i + 1), m));
    }

    // 中心线（点划线）：水平 + 垂直，两端各伸出（图纸量，与比例无关）
    const double ext = 2.0 + 3.0 * s;
    out.push_back(centerLine(Vec3(cx - rOut - ext, cy, 0), Vec3(cx + rOut + ext, cy, 0),
                             QStringLiteral("主视图:水平中心线"), m));
    out.push_back(centerLine(Vec3(cx, cy - rOut - ext, 0), Vec3(cx, cy + rOut + ext, 0),
                             QStringLiteral("主视图:垂直中心线"), m));

    // 螺栓孔中心线（小十字，长 1.6d，GB/T 4457.4）
    if (p.boltCount <= 12) {
        const double cr = rBolt * 1.6;
        for (int i = 0; i < p.boltCount; ++i) {
            const double a = p.boltAngle0Deg + 360.0 * i / p.boltCount;
            const double rad = a * K_PI / 180.0;
            const double bx = cx + rBoltC * std::cos(rad), by = cy + rBoltC * std::sin(rad);
            out.push_back(centerLine(Vec3(bx - cr, by, 0), Vec3(bx + cr, by, 0),
                                     QStringLiteral("主视图:孔中心线"), m));
            out.push_back(centerLine(Vec3(bx, by - cr, 0), Vec3(bx, by + cr, 0),
                                     QStringLiteral("主视图:孔中心线"), m));
        }
    }
}

// ---- 轴测（侧/俯）视公共绘制：把法兰的"轴向剖面矩形"画出来 ----
//   axialHorizontal = true  → 轴向沿水平(X)，直径沿竖直(Y)  → 侧视图（与主视图"高平齐"）
//   axialHorizontal = false → 轴向沿竖直(Y)，直径沿水平(X)  → 俯视图（与主视图"长对正"）
// 几何：盘体（厚 C，直径 D）在"前端"，颈部（高 hubHeight−C，直径 N）在"后端"凸出；
//       中心孔 d 贯穿盘体与颈；螺栓孔（分布圆 K、孔径 L）穿透盘体厚 C。
// 这样画出的轮廓是一个矩形框（含盘/颈台阶，符合"主视图为圆时侧/俯视图必为矩形"的规则）。
void buildProfileView(std::vector<Element>& out, const FlangeParams& p, double s,
                      double cx, double cy, bool axialHorizontal,
                      const QString& prefix, const SketchModel* m) {
    const double AL    = p.hubHeight  * s;   // 轴向总长（盘体厚 C + 颈部凸出）
    const double tC    = p.thickness  * s;   // 盘体厚（前端段）
    const double rOut  = p.outerD     * 0.5 * s;
    const double rHub  = p.hubD       * 0.5 * s;
    const double rBore = p.boreD      * 0.5 * s;
    const double yBolt = p.boltCircle  * 0.5 * s;  // 螺栓分布圆半径（直径方向偏移）
    const double rBolt = p.boltHoleD   * 0.5 * s;

    // 局部坐标 (a = 轴向 0..AL, d = 直径方向偏移 −rd..+rd) → 图纸坐标
    auto P = [&](double a, double d) -> Vec3 {
        if (axialHorizontal) return Vec3(cx + a - AL * 0.5, cy + d, 0);
        return Vec3(cx + d, cy + AL * 0.5 - a, 0);   // 俯视：前端(a=0)在上方
    };
    const QString L_out  = prefix + QStringLiteral(":盘体轮廓");
    const QString L_hub  = prefix + QStringLiteral(":颈部轮廓");
    const QString L_bore = prefix + QStringLiteral(":中心孔(虚线)");
    const QString L_axis = prefix + QStringLiteral(":轴线");

    // ---- 可见轮廓（粗实线）----
    // 依是否带颈部凸台分两支：
    //   带颈（hasHub=true） → 盘体 + 颈部台阶（原结构，保持不变）；
    //   平板（hasHub=false） → 仅盘体包络（轴向总长=厚度，无凸台）。
    // 右端面可带止口凹槽（recess）：凹槽把该处退后 recessDepth，形成
    //   "外缘环带 + 止口壁 + 止口底"三段可见轮廓（止口底与中心孔虚线相接）。
    const double rRecess = p.recessD * 0.5 * s;

    auto drawRecess = [&](double aFace, double rRimOuter) {
        const double aR = aFace - p.recessDepth * s;          // 止口底轴向位置
        out.push_back(solid(P(aFace, +rRimOuter), P(aFace, +rRecess),  prefix + QStringLiteral(":右端面环带"), m));
        out.push_back(solid(P(aFace, +rRecess),  P(aR,   +rRecess),  prefix + QStringLiteral(":止口壁"),     m));
        out.push_back(solid(P(aR,   +rRecess),  P(aR,   +rBore),   prefix + QStringLiteral(":止口底"),     m));
        out.push_back(solid(P(aFace, -rRimOuter), P(aFace, -rRecess),  prefix + QStringLiteral(":右端面环带"), m));
        out.push_back(solid(P(aFace, -rRecess),  P(aR,   -rRecess),  prefix + QStringLiteral(":止口壁"),     m));
        out.push_back(solid(P(aR,   -rRecess),  P(aR,   -rBore),   prefix + QStringLiteral(":止口底"),     m));
    };

    if (p.hasHub) {
        out.push_back(solid(P(0, -rOut), P(0, +rOut), prefix + QStringLiteral(":盘体端面"), m)); // 前端面
        out.push_back(solid(P(0, +rOut), P(tC, +rOut), L_out, m));   // 盘体上缘
        out.push_back(solid(P(0, -rOut), P(tC, -rOut), L_out, m));   // 盘体下缘
        out.push_back(solid(P(tC, +rOut), P(tC, +rHub), L_hub, m));  // 盘体→颈部 台阶(上)
        out.push_back(solid(P(tC, -rOut), P(tC, -rHub), L_hub, m));  // 盘体→颈部 台阶(下)
        out.push_back(solid(P(tC, +rHub), P(AL, +rHub), L_hub, m));  // 颈部上缘
        out.push_back(solid(P(tC, -rHub), P(AL, -rHub), L_hub, m));  // 颈部下缘
        if (p.recessDepth > 1e-6 && rRecess > rBore + 1e-6)
            drawRecess(AL, rHub);
        else
            out.push_back(solid(P(AL, -rHub), P(AL, +rHub), prefix + QStringLiteral(":颈部端面"), m)); // 颈后端面
    } else {
        out.push_back(solid(P(0, -rOut), P(0, +rOut), prefix + QStringLiteral(":盘体端面"), m)); // 左端面
        out.push_back(solid(P(0, +rOut), P(tC, +rOut), L_out, m));   // 盘体上缘
        out.push_back(solid(P(0, -rOut), P(tC, -rOut), L_out, m));   // 盘体下缘
        if (p.recessDepth > 1e-6 && rRecess > rBore + 1e-6)
            drawRecess(tC, rOut);
        else
            out.push_back(solid(P(tC, -rOut), P(tC, +rOut), prefix + QStringLiteral(":盘体端面"), m)); // 右端面
    }

    // ---- 中心孔（通孔内壁，不可见 → 虚线）----
    out.push_back(hidden(P(0, +rBore), P(AL, +rBore), L_bore, m));
    out.push_back(hidden(P(0, -rBore), P(AL, -rBore), L_bore, m));

    // ---- 螺栓孔（穿透盘体厚 tC，沿分布圆，在直径方向投影到 ±yBolt 处）----
    // 仅画偏离轴线那一圈（±yBolt）的两条边；落在轴线上的孔与中心孔重合不另画。
    out.push_back(hidden(P(0, +yBolt - rBolt), P(tC, +yBolt - rBolt), prefix + QStringLiteral(":螺栓孔"), m));
    out.push_back(hidden(P(0, +yBolt + rBolt), P(tC, +yBolt + rBolt), prefix + QStringLiteral(":螺栓孔"), m));
    out.push_back(hidden(P(0, -yBolt - rBolt), P(tC, -yBolt - rBolt), prefix + QStringLiteral(":螺栓孔"), m));
    out.push_back(hidden(P(0, -yBolt + rBolt), P(tC, -yBolt + rBolt), prefix + QStringLiteral(":螺栓孔"), m));

    // ---- 中心线（点划线）：轴线 + 直径方向中心线 ----
    out.push_back(centerLine(P(0, 0), P(AL, 0), L_axis, m));
    out.push_back(centerLine(P(AL * 0.5, -rOut), P(AL * 0.5, +rOut),
                             prefix + QStringLiteral(":直径中心线"), m));
}

} // namespace

// ---------------------------------------------------------------------------
FlangeViewsResult buildFlangeViews(const FlangeParams& pin, const SketchModel* model) {
    FlangeParams p = pin;
    p.makeValid();

    FlangeViewsResult R;
    const QRectF area = p.availableArea();
    const double gapH = std::max(p.gap, 6.0);     // 主视图—侧视图 水平净间距（用户要求增大）
    const double gapV = gapH + 10.0;              // 主视图—俯视图 竖直净间距（额外留 10mm 给标注）
    LayoutPlan L = planLayout(p, area.width(), area.height(), gapH, gapV);
    if (!p.autoScale) { L = planLayout(p, area.width(), area.height(), gapH, gapV);
                        L.scale = p.manualScale; }

    const double s = L.scale;
    const double rOut = p.outerD * 0.5 * s;   // = 主视图外圆半径 = 侧/俯视图直径半高
    const double AL   = p.hubHeight * s;       // 轴向总长 = 侧视图宽 = 俯视图高
    const double dh   = p.dimTextHeight;       // 缩小后的标注字高
    const double dimOff = 6.0 + 8.0 * s;      // 尺寸线离视图的距离（随比例，但比旧版更紧凑）

    // ---- 定位（用户指定布局：主视图与侧视图并排于上方，俯视图在主视图正下方）----
    //   主视图（圆，左上）→ 侧视图（矩形，右上，与主视图"高平齐"）
    //   俯视图（矩形，位于主视图正下方，"长对正"）
    //   竖直方向：俯视图在上、主视图在下；整块以俯视图顶边 blockTopY 为锚点做竖直居中。
    const double kAnnoTop = 9.0;   // 上方：俯视图外径标注(~5) + 余量
    const double kAnnoBot = 9.0;   // 下方：主/侧视图外径标注(~5) + 余量
    const double contentW = 2.0 * rOut + gapH + AL;          // 内容宽（主视图直径 + 主侧间距 + 侧视图轴向）
    const double blockH   = AL + gapV + 2.0 * rOut;          // 纯几何块高（俯视图轴向 + 间距 + 主视图直径）
    const double totalH   = blockH + kAnnoTop + kAnnoBot;    // 含上下标注余量

    const double x0       = area.left() + std::max(0.0, (area.width() - contentW) * 0.5);
    const double blockTopY = area.top() + std::max(0.0, (area.height() - totalH) * 0.5) + kAnnoTop;

    // 俯视图中心（上方，与主视图共竖直轴"长对正"）
    const double topCx   = x0 + rOut;
    const double topCy   = blockTopY + AL * 0.5;
    // 主视图中心（下方，与侧视图"高平齐"）
    const double frontCx = topCx;
    const double frontCy = blockTopY + AL + gapV + rOut;
    // 侧视图中心（右上，与主视图等高）
    const double sideCx  = frontCx + rOut + gapH + AL * 0.5;
    const double sideCy  = frontCy;

    R.scale = s;
    R.scaleLabel = scaleLabel(s);
    R.frontCx = frontCx; R.frontCy = frontCy;
    R.topCx = topCx;     R.topCy = topCy;
    R.sideCx = sideCx;   R.sideCy = sideCy;
    R.frontR = rOut; R.profHalfA = AL * 0.5; R.profHalfD = rOut;

    // ---- 生成三个视图 ----
    buildFrontView  (R.elems, p, s, frontCx, frontCy, model);
    buildProfileView(R.elems, p, s, sideCx, sideCy, true,  QStringLiteral("侧视图"), model);
    buildProfileView(R.elems, p, s, topCx,  topCy,  false, QStringLiteral("俯视图"), model);

    // ---- 视图名：按出图要求不绘制（"主视图/俯视图/侧视图"提示字不出现在图纸上）----
    //   三视图的投影关系完全由几何对齐（长对正/高平齐/宽相等）表达。
    //   注：原先在此处生成的 4 条「投影辅助线」（视图间隙内的点划线）按需求已全部删除，
    //       视图之间不再有任何引导线/连系线。

    // ---- 尺寸标注（数值为**实物尺寸**；DimStyle 以绘制比例还原，见 model->dimStyle）----
    // 主视图①外径 D：水平尺寸线，放在视图上方
    R.elems.push_back(mkDim(DimensionType::Horizontal,
                            Vec3(frontCx - rOut, frontCy + rOut, 0),
                            Vec3(frontCx + rOut, frontCy + rOut, 0),
                            Vec3(frontCx, frontCy + rOut + dimOff * 0.7, 0),
                            dimD(p.outerD), model, dh));
    // 主视图②螺栓孔分布圆直径 K：竖直尺寸线，放在视图左侧
    R.elems.push_back(mkDim(DimensionType::Vertical,
                            Vec3(frontCx, frontCy - p.boltCircle * 0.5 * s, 0),
                            Vec3(frontCx, frontCy + p.boltCircle * 0.5 * s, 0),
                            Vec3(frontCx - rOut - dimOff, frontCy, 0),
                            dimD(p.boltCircle), model, dh));
    // 主视图③中心孔径：水平尺寸线，放在视图中心（不额外占空间）
    R.elems.push_back(mkDim(DimensionType::Horizontal,
                            Vec3(frontCx - p.boreD * 0.5 * s, frontCy, 0),
                            Vec3(frontCx + p.boreD * 0.5 * s, frontCy, 0),
                            Vec3(frontCx, frontCy, 0),
                            dimD(p.boreD) + p.boreTol, model, dh));
    // 注：螺栓孔数量×孔径、止口外径原本用「引线标注」(mkLeader) 引出——按需求移除
    //     视图间的标注/对应关系引导线，故此处不再生成引线（mkLeader）。两类尺寸改为
    //     仅在尺寸线上注写；如需恢复，可重新用 mkLeader 添加（见下方 mkDim 同类调用）。
    // 侧视图⑤法兰厚度 C：水平尺寸线，放在视图上方（盘体顶面）
    R.elems.push_back(mkDim(DimensionType::Horizontal,
                            Vec3(sideCx - AL * 0.5, sideCy + rOut, 0),
                            Vec3(sideCx - AL * 0.5 + p.thickness * s, sideCy + rOut, 0),
                            Vec3(sideCx - AL * 0.5 + p.thickness * 0.5 * s,
                                 sideCy + rOut + dimOff * 0.7, 0),
                            n2(p.thickness) + p.thicknessTol, model, dh));
    // 侧视图⑥颈部总高：水平尺寸线，放在视图上方更外层
    //   仅带颈法兰标注（平板法兰 hubHeight=厚度，与⑤重复，故省略）
    if (p.hasHub) {
    R.elems.push_back(mkDim(DimensionType::Horizontal,
                            Vec3(sideCx - AL * 0.5, sideCy + rOut, 0),
                            Vec3(sideCx + AL * 0.5, sideCy + rOut, 0),
                            Vec3(sideCx, sideCy + rOut + dimOff * 1.5, 0),
                            n2(p.hubHeight), model, dh));
    // 侧视图⑦颈部外径 N：竖直尺寸线，放在视图右侧
    R.elems.push_back(mkDim(DimensionType::Vertical,
                            Vec3(sideCx + AL * 0.5, sideCy - p.hubD * 0.5 * s, 0),
                            Vec3(sideCx + AL * 0.5, sideCy + p.hubD * 0.5 * s, 0),
                            Vec3(sideCx + AL * 0.5 + dimOff, sideCy, 0),
                            dimD(p.hubD), model, dh));
    }

    // 侧视图⑨止口深度（凹槽轴向深度）：水平尺寸线，放在侧视图下方（视图外侧，
    //   用引出线伸入止口壁与中心孔之间的实心区，避免被小的 5mm 特征压住）。
    //   止口所在端面轴向坐标 aFaceM：平板法兰 = 厚度 C；带颈法兰 = 颈部总高。
    if (p.recessDepth > 1e-6 && p.recessD > 1e-6) {
        const double rRecess = p.recessD * 0.5 * s;
        const double rBoreS  = p.boreD  * 0.5 * s;
        const double rMid    = (rRecess + rBoreS) * 0.5;   // 止口壁与中心孔之间的实心区中线上
        const double aFaceM  = p.hasHub ? p.hubHeight : p.thickness;   // 止口端面（模型轴向）
        const double xFace   = sideCx + aFaceM * s - AL * 0.5;          // 端面 x（图纸）
        const double xBot    = sideCx + (aFaceM - p.recessDepth) * s - AL * 0.5; // 止口底 x
        R.elems.push_back(mkDim(DimensionType::Horizontal,
                                Vec3(xFace, sideCy + rMid, 0),
                                Vec3(xBot,  sideCy + rMid, 0),
                                Vec3((xFace + xBot) * 0.5, sideCy - rOut - dimOff * 0.7, 0),
                                n2(p.recessDepth), model, dh));
        // 止口外径（原由引线标注 Ø100h7 引出；按需求移除引线后改为正式直径尺寸线，
        // 公差 recessTol 随标注文本给出，信息不丢失）。
        R.elems.push_back(mkDim(DimensionType::Vertical,
                                Vec3(frontCx + rOut + dimOff, frontCy - rRecess, 0),
                                Vec3(frontCx + rOut + dimOff, frontCy + rRecess, 0),
                                Vec3(frontCx + rOut + dimOff * 2.0, frontCy, 0),
                                dimD(p.recessD) + p.recessTol, model, dh));
    }

    // 俯视图⑧外径 D：水平尺寸线，放在视图上方
    R.elems.push_back(mkDim(DimensionType::Horizontal,
                            Vec3(topCx - rOut, topCy + AL * 0.5, 0),
                            Vec3(topCx + rOut, topCy + AL * 0.5, 0),
                            Vec3(topCx, topCy + AL * 0.5 + dimOff * 0.7, 0),
                            dimD(p.outerD), model, dh));

    // ---- 说明文字：关键规格摘要，放在可用区左上角外侧，避开图框与标题栏 ----
    const QString note = QStringLiteral("法兰 %1 | 孔 %2×%3 | 比例 %4")
                             .arg(dimD(p.outerD)).arg(p.boltCount).arg(dimD(p.boltHoleD))
                             .arg(R.scaleLabel);
    R.elems.push_back(mkText(Vec3(area.left(), area.top() + 4.0, 0), note, 3.5, false));

    // ---- 将中心线（点划线）裁剪到图框内 ----
    // 法：中心线均为轴对齐线段。直接将两端点夹进图框矩形，即使整条线落在图框外也能收回来
    //     （小规格法兰可用区极窄时可能出现）。避免旧版"每端最多缩短半条线"导致的裁剪失效。
    const QRectF frame(p.marginL, p.marginC,
                       p.sheetW - p.marginC - p.marginL,
                       p.sheetH - p.marginC - p.marginC);
    const auto clampTo = [](double v, double lo, double hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    for (auto& e : R.elems) {
        if (e.type != ElementType::Line || e.layer != LY_CENTER || e.pts.size() < 2) continue;
        Vec3& a = e.pts[0];
        Vec3& b = e.pts[1];
        if (qFuzzyIsNull(a.x - b.x)) {                 // 竖直线：X 夹进图框，Y 两端夹进图框
            a.x = b.x = clampTo(a.x, frame.left(), frame.right());
            const double y1 = clampTo(a.y, frame.top(), frame.bottom());
            const double y2 = clampTo(b.y, frame.top(), frame.bottom());
            a.y = std::min(y1, y2); b.y = std::max(y1, y2);
        } else if (qFuzzyIsNull(a.y - b.y)) {          // 水平线：Y 夹进图框，X 两端夹进图框
            a.y = b.y = clampTo(a.y, frame.top(), frame.bottom());
            const double x1 = clampTo(a.x, frame.left(), frame.right());
            const double x2 = clampTo(b.x, frame.left(), frame.right());
            a.x = std::min(x1, x2); b.x = std::max(x1, x2);
        }
        if (qFuzzyIsNull(a.x - b.x) && qFuzzyIsNull(a.y - b.y)) {
            b.x = a.x + 1.0;   // 退化成点：拉长成 1mm 小线段，避免零长线段在渲染/包围盒里消失
        }
    }

    // ---- 统计与边界 ----
    for (const auto& e : R.elems)
        if (e.type == ElementType::Dimension) {
            if (e.dtype == DimensionType::Leader) { /* 已单独计数 */ }
            else ++R.dimCount;
        }

    QRectF b;
    bool has = false;
    for (const auto& e : R.elems) {
        const QRectF eb = geo::elementBounds(e);
        b = has ? b.united(eb) : eb; has = true;
    }
    R.bounds = b;
    R.area = area;
    R.fits = area.contains(b);
    return R;
}

QString flangeViewTitle(int index) {
    switch (index) {
        case 0: return QStringLiteral("主视图");
        case 1: return QStringLiteral("俯视图");
        case 2: return QStringLiteral("侧视图");
        default: return QString();
    }
}

} // namespace SketchCAD
