#include "DimStyle.h"
#include "Geometry.h"
#include "SketchModel.h"
#include "TextStyle.h"      // ResolvedTextStyle
#include "TextMetrics.h"    // 标注文字宽度：真实度量（有 GUI）/ 无头确定性估算
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SketchCAD {
namespace dim {

// ---------------------------- 小工具 ----------------------------

QString diameterSign() { return QString(QChar(0x2300)); }   // ⌀ U+2300

QString arcLengthSign() { return QStringLiteral("⌒"); }

// 按标准策略把尺寸线方向角转成文字旋转角（度）。
// 注意：返回值随后会被 painter 的 -1 缩放 Y 轴影响，实际 render 时再取 -textRotDeg。
double textRotationForLine(double lineAngleDeg, TextOrientation policy) {
    double a = geo::wrapDeg180(lineAngleDeg);
    switch (policy) {
        case TextOrientation::Horizontal:
            return 0.0;
        case TextOrientation::AlignedISO:
            // ISO：文字平行于尺寸线，保证字头大致朝上（屏幕可读）。
            if (a > 90.0)       a -= 180.0;
            else if (a <= -90.0) a += 180.0;
            return a;
        case TextOrientation::AlignedReadable:
        default: {
            // GB/T 4458.4-2003 方法 1：数字按可读方向，避免 30° 范围内倒置。
            // 先把文字限制在 (-90°, 90°]，再对接近垂直的角度统一为竖直可读方向。
            if (a > 90.0)       a -= 180.0;
            else if (a <= -90.0) a += 180.0;
            if (a > 60.0)        a = 90.0;
            else if (a < -60.0)  a = -90.0;
            return a;
        }
    }
}

QString formatValue(const Element& e, double m) {
    if (!e.text.isEmpty()) return e.text;                   // 用户手改过就以手改为准
    switch (e.dtype) {
        case DimensionType::Radius:   return QStringLiteral("R") + QString::number(m, 'f', 2);
        case DimensionType::Diameter: return diameterSign() + QString::number(m, 'f', 2);
        case DimensionType::Angle:    return QString::number(m, 'f', 2) + QStringLiteral("°");
        case DimensionType::Arc:      return arcLengthSign() + QString::number(m, 'f', 2);
        case DimensionType::Leader:   return e.text.isEmpty() ? QStringLiteral("?") : e.text;
        case DimensionType::Ordinate: return QString::number(m, 'f', 2);
        default:                      return QString::number(m, 'f', 2);
    }
}

QString formatTolerance(const Element& e, double nominal, const Style& st) {
    // 公差注写规则：默认按 e.standard 取（保持与命令/工具/UI 一致）；
    // 当 Style.stdConf.standard 与 e.standard 不一致时，表示"被 Style 覆盖"，
    // 此时按 Style 的 toleranceStyle 取（满足单标注覆盖 + 全局切换混用）。
    const ToleranceStyle ts = (st.stdConf.standard != e.standard)
        ? st.stdConf.toleranceStyle
        : DimensionStandardConfig::forStandard(e.standard).toleranceStyle;
    switch (ts) {
        case ToleranceStyle::None:
            return QString();
        case ToleranceStyle::Symmetric:
            if (std::fabs(e.tolUpper) < 1e-12) return QString();
            return QStringLiteral(" ±") + QString::number(e.tolUpper, 'f', 2);
        case ToleranceStyle::DeviationStacked:
            if (std::fabs(e.tolUpper) < 1e-12 && std::fabs(e.tolLower) < 1e-12) return QString();
            return QString(QStringLiteral("(+%1/-%2)"))
                   .arg(e.tolUpper, 0, 'f', 2)
                   .arg(std::fabs(e.tolLower), 0, 'f', 2);
        case ToleranceStyle::LimitSize:
            if (std::fabs(e.tolUpper) > 1e-12)
                return QStringLiteral(" ") + QString::number(nominal + e.tolUpper, 'f', 2) + QStringLiteral(" max.");
            if (std::fabs(e.tolLower) > 1e-12)
                return QStringLiteral(" ") + QString::number(nominal - std::fabs(e.tolLower), 'f', 2) + QStringLiteral(" min.");
            return QString();
        case ToleranceStyle::FitCode:
            if (e.fitCode.isEmpty()) return QString();
            return QStringLiteral(" ") + e.fitCode;
    }
    return QString();
}

// 在 tip 处放一个箭头；outside=true 表示尺寸过小、箭头画到尺寸界线外侧并反向指入
static void putArrow(Geom& g, const Vec3& tip, const Vec3& towardTip, bool outside) {
    Arrow a;
    a.tip = tip;
    a.dirDeg = (outside ? towardTip * -1.0 : towardTip).angleDeg();
    g.arrows.push_back(a);
}

// ---------------------------- 线性 / 对齐 / 水平 / 竖直 / 基线 / 连续 ----------------------------

// 图上实测 → 实物尺寸。
// GB/T 14690：比例只改变图形（要素的线性尺寸）大小，**不改变标注数值**——
// 1:2 图纸上实物 100mm 的零件画成 50mm，但标注必须仍写 100。
// 模型里 pts 存的就是图纸坐标，所以读数要把比例因子除回去。
// 角度量（°）与比例无关，调用方不要走这里。
static double toTrueSize(double measuredPaper, const Style& st) {
    return (st.drawingScale > 1e-12) ? measuredPaper / st.drawingScale : measuredPaper;
}

static Geom buildLinearInternal(const Element& e, const Style& st,
                                const Vec3& A, const Vec3& B,
                                const Vec3& POverride, bool useOverride) {
    Geom g;
    if (e.pts.size() < 2) return g;

    // 尺寸线方向
    Vec3 dir;
    switch (e.dtype) {
        case DimensionType::Aligned:
        case DimensionType::Baseline:
        case DimensionType::Continue:
            dir = (B - A).normalized();
            break;
        case DimensionType::Horizontal:
            dir = Vec3(1, 0, 0);
            break;
        case DimensionType::Vertical:
            dir = Vec3(0, 1, 0);
            break;
        default:
            dir = (std::fabs(B.x - A.x) >= std::fabs(B.y - A.y)) ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
            break;
    }
    const Vec3 n = dir.perp();

    // 尺寸线通过点
    Vec3 P = useOverride ? POverride
                       : ((e.pts.size() >= 3) ? e.pts[2]
                                               : (A + B) * 0.5 + n * st.defOffset);

    // 把 A、B 投影到「过 P、方向 dir」的那条直线上
    const Vec3 A2 = A + n * (P - A).dot2D(n);
    const Vec3 B2 = B + n * (P - B).dot2D(n);

    g.measured = std::fabs((B - A).dot2D(dir));
    if (g.measured < 1e-9) return g;

    // 尺寸界线
    auto ext = [&](const Vec3& src, const Vec3& dst) {
        const Vec3 v = dst - src;
        const double L = std::hypot(v.x, v.y);
        if (L < 1e-9) return;
        const Vec3 u = v * (1.0 / L);
        g.ext.push_back({src + u * st.extGap, dst + u * st.extBeyond});
    };
    ext(A, A2);
    ext(B, B2);

    const Vec3 u = (B2 - A2).normalized();
    const bool tiny = (g.measured < 3.0 * st.arrowLen);
    if (tiny) {
        const double outLen = 2.2 * st.arrowLen;
        g.line.push_back({A2 - u * outLen, B2 + u * outLen});
    } else {
        g.line.push_back({A2, B2});
    }
    putArrow(g, A2, u * -1.0, tiny);
    putArrow(g, B2, u, tiny);

    g.textRotDeg = textRotationForLine(u.angleDeg(), st.textOrientation());
    const Vec3 rd = math::unitOf(g.textRotDeg);
    const Vec3 nr = rd.perp();
    Vec3 mid = (A2 + B2) * 0.5;
    g.textPos = mid + nr * (st.textGap + st.textHeight * 0.5);
    g.value = toTrueSize(g.measured, st);            // 图上距离 → 实物尺寸
    g.label = formatValue(e, g.value);
    g.tolLabel = formatTolerance(e, g.value, st);
    g.valid = true;
    return g;
}

static Geom buildLinear(const Element& e, const Style& st) {
    if (e.pts.size() < 2) return Geom();
    return buildLinearInternal(e, st, e.pts[0], e.pts[1], Vec3(), false);
}

static Geom buildBaseline(const Element& e, const Style& st) {
    if (e.pts.size() < 2) return Geom();
    // 基线标注：用 e.radius 存储层叠偏移倍数，基准偏移 = defOffset + radius * (textHeight+4)
    const double offset = st.defOffset + e.radius * (st.textHeight + 4.0);
    const Vec3 A = e.pts[0];
    const Vec3 B = e.pts[1];
    Vec3 dir = (B - A).normalized();
    if (e.dtype == DimensionType::Horizontal) dir = Vec3(1, 0, 0);
    if (e.dtype == DimensionType::Vertical)   dir = Vec3(0, 1, 0);
    Vec3 n = dir.perp();
    // 让基线组统一向测量线同一侧偏置
    Vec3 P = (A + B) * 0.5 + n * offset;
    return buildLinearInternal(e, st, A, B, P, true);
}

static Geom buildContinue(const Element& e, const Style& st) {
    if (e.pts.size() < 2) return Geom();
    // 连续标注：尺寸线从 A 开始，到 B 结束，沿 dir 方向，偏移位置由 e.radius 控制
    const double offset = st.defOffset + e.radius * (st.textHeight + 4.0);
    const Vec3 A = e.pts[0];
    const Vec3 B = e.pts[1];
    Vec3 dir = (B - A).normalized();
    Vec3 n = dir.perp();
    Vec3 P = (A + B) * 0.5 + n * offset;
    return buildLinearInternal(e, st, A, B, P, true);
}

// ---------------------------- 角度 ----------------------------

static Geom buildAngle(const Element& e, const Style& st) {
    Geom g;
    if (e.pts.size() < 3) return g;
    const Vec3 V = e.pts[0];
    const Vec3 u1 = (e.pts[1] - V).normalized();
    const Vec3 u2 = (e.pts[2] - V).normalized();
    const double L1 = std::hypot(e.pts[1].x - V.x, e.pts[1].y - V.y);
    const double L2 = std::hypot(e.pts[2].x - V.x, e.pts[2].y - V.y);
    if (L1 < 1e-9 || L2 < 1e-9) return g;

    const double a1 = u1.angleDeg();
    const double sweep = geo::wrapDeg180(u2.angleDeg() - a1);
    g.measured = std::fabs(sweep);
    if (g.measured < 1e-6) return g;

    const double R = (e.radius > 1e-6) ? e.radius : 0.6 * std::min(L1, L2);

    auto radialExt = [&](const Vec3& u, double L) {
        const double need = R + st.extBeyond;
        if (need <= L + 1e-9) return;
        g.ext.push_back({V + u * L, V + u * need});
    };
    radialExt(u1, L1);
    radialExt(u2, L2);

    const int steps = std::max(2, (int)std::ceil(g.measured / st.arcSeg));
    g.arcPoly.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i)
        g.arcPoly.push_back(V + math::unitOf(a1 + sweep * i / steps) * R);

    const double sgn = (sweep >= 0) ? 1.0 : -1.0;
    putArrow(g, g.arcPoly.front(), math::unitOf(a1 - 90.0 * sgn), false);
    putArrow(g, g.arcPoly.back(),  math::unitOf(a1 + sweep + 90.0 * sgn), false);

    // 角度数字按标准策略；GB/T/ISO 一般均水平，但允许策略覆盖
    g.textRotDeg = (st.textOrientation() == TextOrientation::AlignedISO)
                       ? textRotationForLine(a1 + sweep * 0.5, TextOrientation::AlignedISO)
                       : 0.0;
    g.textPos = V + math::unitOf(a1 + sweep * 0.5) * R;
    if (st.textOrientation() == TextOrientation::Horizontal) {
        // 文字微抬，避免压在弧线上
        g.textPos = g.textPos + math::unitOf(g.textRotDeg).perp() * (st.textGap + st.textHeight * 0.5);
    }
    // 角度是「形状」量，等比缩放下不变，故不换算比例（value == measured）
    g.value = g.measured;
    g.label = formatValue(e, g.value);
    g.tolLabel = formatTolerance(e, g.value, st);
    g.valid = true;
    return g;
}

// ---------------------------- 弧长 ----------------------------

static Geom buildArcLength(const Element& e, const Style& st) {
    Geom g;
    // 弧长标注的实测值依赖 e.pts[0]=圆心, e.radius=半径, startAngle/endAngle=起止角。
    if (e.pts.empty() || e.radius <= 1e-9) return g;
    const Vec3 C = e.pts[0];
    const double R = e.radius;
    const double a0 = e.startAngle;
    const double sweep = geo::wrapDeg180(e.endAngle - a0);
    g.measured = R * std::fabs(sweep) * M_PI / 180.0;
    if (g.measured < 1e-9) return g;

    // 文字定位：圆弧中点切向为文字基线方向
    const double aMid = a0 + sweep * 0.5;
    const Vec3 uMid = math::unitOf(aMid);

    // 尺寸线：与圆弧同心的内侧/外侧弧，默认放在 0.85 R 处
    const double rDim = R * 0.85;
    const int steps = std::max(2, (int)std::ceil(std::fabs(sweep) / st.arcSeg));
    g.arcPoly.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i)
        g.arcPoly.push_back(C + math::unitOf(a0 + sweep * i / steps) * rDim);

    // 界线：从圆弧端点引向尺寸弧端点
    g.ext.push_back({C + math::unitOf(a0) * R, C + math::unitOf(a0) * rDim});
    g.ext.push_back({C + math::unitOf(a0 + sweep) * R, C + math::unitOf(a0 + sweep) * rDim});

    // 箭头沿切向
    const double sgn = (sweep >= 0) ? 1.0 : -1.0;
    putArrow(g, g.arcPoly.front(), math::unitOf(a0 - 90.0 * sgn), false);
    putArrow(g, g.arcPoly.back(),  math::unitOf(a0 + sweep + 90.0 * sgn), false);

    g.textPos = C + uMid * rDim + uMid.perp() * (st.textGap + st.textHeight * 0.5);
    g.textRotDeg = textRotationForLine((uMid.perp() * sgn).angleDeg(), st.textOrientation());
    g.value = toTrueSize(g.measured, st);            // 弧长是长度量，随比例换算
    g.label = formatValue(e, g.value);
    g.tolLabel = formatTolerance(e, g.value, st);
    g.valid = true;
    return g;
}

// ---------------------------- 引线 ----------------------------

static Geom buildLeader(const Element& e, const Style& st) {
    Geom g;
    if (e.pts.size() < 2) return g;
    const Vec3 arrowTip = e.pts[0];
    const Vec3 knee     = e.pts[1];
    g.line.push_back({arrowTip, knee});
    Vec3 textAnchor = knee;
    if (e.pts.size() >= 3) {
        textAnchor = e.pts[2];
        g.line.push_back({knee, textAnchor});
    }
    Vec3 dir = (knee - arrowTip).normalized();
    if (dir.len2D() < 1e-9) dir = Vec3(1, 0, 0);
    putArrow(g, arrowTip, dir, false);

    g.textPos = textAnchor + Vec3(0, st.textGap + st.textHeight * 0.5, 0);
    g.textRotDeg = 0.0;
    g.textOnShelf = true;
    g.label = e.text.isEmpty() ? QStringLiteral("?") : e.text;
    g.measured = 0.0;
    g.value = 0.0;                  // 引线标注无尺寸数字（label 取用户文本）
    g.valid = true;
    return g;
}

// ---------------------------- 坐标标注 ----------------------------

static Geom buildOrdinate(const Element& e, const Style& st) {
    Geom g;
    if (e.pts.size() < 2) return g;
    const Vec3 feature = e.pts[0];   // 被测点
    const Vec3 datum   = e.pts[1];   // 坐标基准点
    // e.rotation 0/360/180 表示 X 坐标；90/-90 表示 Y 坐标
    const bool isX = (std::fabs(std::sin(math::deg2rad(e.rotation))) < 0.707);
    const Vec3 dir = isX ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
    const double value = isX ? (feature.x - datum.x) : (feature.y - datum.y);
    g.measured = std::fabs(value);

    // 坐标引线：从被测点沿 dir 方向引出到 datum 的垂足，再继续到标注位置
    const Vec3 proj = datum + dir * ((feature - datum).dot2D(dir));
    const Vec3 n = dir.perp();
    const double extLen = st.defOffset * 0.6;
    const Vec3 labelPos = proj + n * extLen + dir * (value >= 0 ? 1.0 : -1.0) * st.textHeight * 0.5;

    g.line.push_back({feature, proj});
    g.line.push_back({proj, labelPos});
    putArrow(g, feature, (feature - proj).normalized(), false); // 小圆点由 render 处理

    g.textPos = labelPos + Vec3(0, st.textGap + st.textHeight * 0.5, 0);
    g.textRotDeg = 0.0;
    g.textOnShelf = true;
    g.value = toTrueSize(std::fabs(value), st);      // 坐标差也是长度量，随比例换算
    g.label = QString::number(g.value * (value < 0 ? -1.0 : 1.0), 'f', 2);
    g.tolLabel = formatTolerance(e, g.value, st);
    g.valid = true;
    return g;
}

// ---------------------------- 半径 / 直径 ----------------------------

static Geom buildRadial(const Element& e, const Style& st, bool diameter) {
    Geom g;
    if (e.pts.empty() || e.radius <= 1e-9) return g;
    const Vec3 C = e.pts[0];
    const double R = e.radius;
    const Vec3 u = (e.pts.size() >= 2 && (e.pts[1] - C).len() > 1e-9)
                       ? (e.pts[1] - C).normalized()
                       : math::unitOf(45.0);

    g.measured = diameter ? 2.0 * R : R;
    g.value = toTrueSize(g.measured, st);            // 半径/直径随比例换算
    g.label = formatValue(e, g.value);
    g.tolLabel = formatTolerance(e, g.value, st);

    const Vec3 Pout = C + u * R;
    const bool tiny = (R < 3.0 * st.arrowLen);

    if (diameter) {
        const Vec3 Pin = C - u * R;
        if (tiny) {
            const double outLen = 2.2 * st.arrowLen;
            g.line.push_back({Pin - u * outLen, Pout + u * outLen});
        } else {
            g.line.push_back({Pin, Pout});
        }
        putArrow(g, Pin,  u * -1.0, tiny);
        putArrow(g, Pout, u, tiny);
        g.textRotDeg = textRotationForLine(u.angleDeg(), st.textOrientation());
        g.textPos = C;
    } else if (!tiny) {
        g.line.push_back({C, Pout});
        putArrow(g, Pout, u, false);
        g.textRotDeg = textRotationForLine(u.angleDeg(), st.textOrientation());
        const Vec3 rd = math::unitOf(g.textRotDeg);
        g.textPos = (C + Pout) * 0.5 + rd.perp() * (st.textGap + st.textHeight * 0.5);
    } else {
        const Vec3 knee = C + u * (R + 4.0 * st.arrowLen);
        // 标注文字宽度改用真实度量（无头降级为确定性估算，系数 0.62 与旧一致）。
        ResolvedTextStyle rs; rs.height = st.textHeight;
        const double textW = TextMetrics::lineWidthMm(rs, g.label + g.tolLabel, st.textHeight);
        const double shelf = std::max(st.textHeight * 2.0, textW);
        const double sx = (u.x >= 0) ? 1.0 : -1.0;
        const Vec3 tail = knee + Vec3(shelf * sx, 0, 0);
        g.line.push_back({Pout, knee});
        g.line.push_back({knee, tail});
        putArrow(g, Pout, u * -1.0, false);
        g.textRotDeg = 0.0;
        g.textOnShelf = true;
        g.textPos = (knee + tail) * 0.5 +
                    Vec3(0, st.textGap + st.textHeight * 0.5, 0);
    }
    g.valid = true;
    return g;
}

// ---------------------------- 入口 ----------------------------

Geom build(const Element& e, const Style& st) {
    if (e.type != ElementType::Dimension) return Geom();
    switch (e.dtype) {
        case DimensionType::Angle:    return buildAngle(e, st);
        case DimensionType::Radius:   return buildRadial(e, st, false);
        case DimensionType::Diameter: return buildRadial(e, st, true);
        case DimensionType::Baseline: return buildBaseline(e, st);
        case DimensionType::Continue: return buildContinue(e, st);
        case DimensionType::Arc:      return buildArcLength(e, st);
        case DimensionType::Leader:   return buildLeader(e, st);
        case DimensionType::Ordinate: return buildOrdinate(e, st);
        default:                      return buildLinear(e, st);
    }
}

// ============================================================================
// 命名标注样式表：name → Style 多实例化的序列化
// ============================================================================
QJsonObject dimStyleEntryToJson(const DimStyleEntry& e) {
    QJsonObject o;
    o["name"] = e.name;
    const Style& s = e.style;
    o["arrowLen"]    = s.arrowLen;
    o["arrowWidth"]  = s.arrowWidth;
    o["extBeyond"]   = s.extBeyond;
    o["extGap"]      = s.extGap;
    o["textHeight"]  = s.textHeight;
    o["textGap"]     = s.textGap;
    o["defOffset"]   = s.defOffset;
    o["arcSeg"]      = s.arcSeg;
    // 标准配置四元组（枚举按整数存，顺序只增不改）
    o["std"]         = (int)s.stdConf.standard;
    o["arrowForm"]   = (int)s.stdConf.arrowForm;
    o["textOrient"]  = (int)s.stdConf.textOrientation;
    o["tolStyle"]    = (int)s.stdConf.toleranceStyle;
    return o;
}

DimStyleEntry dimStyleEntryFromJson(const QJsonObject& o) {
    DimStyleEntry e;
    e.name = o["name"].toString("Standard");
    Style& s = e.style;
    // 缺键回落默认（与 Style 的成员默认值一致）
    s.arrowLen   = json::getDouble(o, "arrowLen",   3.5);
    s.arrowWidth = json::getDouble(o, "arrowWidth", 1.1);
    s.extBeyond  = json::getDouble(o, "extBeyond",  2.5);
    s.extGap     = json::getDouble(o, "extGap",     0.0);
    s.textHeight = json::getDouble(o, "textHeight", 3.5);
    s.textGap    = json::getDouble(o, "textGap",    0.8);
    s.defOffset  = json::getDouble(o, "defOffset",  12.0);
    s.arcSeg     = json::getDouble(o, "arcSeg",     4.0);
    s.stdConf.standard         = (DimensionStandard)  json::getInt(o, "std",        (int)DimensionStandard::GBT);
    s.stdConf.arrowForm        = (ArrowForm)          json::getInt(o, "arrowForm",  (int)ArrowForm::ClosedFilled);
    s.stdConf.textOrientation  = (TextOrientation)    json::getInt(o, "textOrient", (int)TextOrientation::AlignedReadable);
    s.stdConf.toleranceStyle   = (ToleranceStyle)     json::getInt(o, "tolStyle",   (int)ToleranceStyle::None);
    // drawingScale 不随样式存盘：它来自图纸比例（SketchModel::dimStyle 每次注入）
    s.drawingScale = 1.0;
    return e;
}

} // namespace dim
} // namespace SketchCAD
