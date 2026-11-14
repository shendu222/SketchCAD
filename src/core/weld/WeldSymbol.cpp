#include "WeldSymbol.h"   // 同目录（src/core/weld/）
#include <cmath>
#include <algorithm>

// ============================================================================
// 焊缝符号几何生成（纯逻辑，无头可测；全部输出既有图元）
// 局部坐标系：膝点(指引线/基准线交点)在原点 (0,0)；基准线沿 +X；指引线自原点
// 向左下 45° 延伸至箭头尖端(接头)。最后整体按 rotationDeg 绕 anchor 旋转后平移。
// ============================================================================
namespace SketchCAD {

namespace {

constexpr double kWeldLineScale = 2.0 / 3.0;   // 线宽 = 2d/3

// 数值格式化：至多 1 位小数，去除尾随零与小数点
QString weldFmt(double v) {
    QString s = QString::number(v, 'f', 1);
    if (s.contains(QChar('.'))) {
        while (s.endsWith(QChar('0'))) s.chop(1);
        if (s.endsWith(QChar('.'))) s.chop(1);
    }
    return s;
}

// 构造一根线段图元（Line），线宽固定为 w（不随层派生）
Element makeSeg(const Vec3& a, const Vec3& b, double w, const QString& layer) {
    Element e;
    e.type = ElementType::Line;
    e.pts = { a, b };
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = false;
    e.lineWidth = w;
    e.byLayer = false;
    return e;
}

// 构造折线图元（Polyline）
Element makePoly(const std::vector<Vec3>& pts, bool closed, double w, const QString& layer) {
    Element e;
    e.type = ElementType::Polyline;
    e.pts = pts;
    e.closed = closed;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = false;
    e.lineWidth = w;
    e.byLayer = false;
    return e;
}

// 构造圆图元（Circle）
Element makeCircle(const Vec3& c, double r, double w, const QString& layer) {
    Element e;
    e.type = ElementType::Circle;
    e.pts = { c };
    e.radius = r;
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = false;
    e.lineWidth = w;
    e.byLayer = false;
    return e;
}

// 构造文本图元（Text）
Element makeText(const Vec3& pos, const QString& str, double h, const QString& layer) {
    Element e;
    e.type = ElementType::Text;
    e.pts = { pos };
    e.radius = h;            // Text 字高复用 radius 字段
    e.text = str;
    e.textAlign = 4;         // 中心对齐（锚点=中心）
    e.layer = layer;
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = false;
    e.lineWidth = 0.5;
    e.byLayer = false;
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// 类型名表（中文 UI 下拉；顺序与 WeldType 枚举对应）
// ---------------------------------------------------------------------------
QStringList weldTypeNames() {
    return { QStringLiteral("角焊缝"), QStringLiteral("V形焊缝"), QStringLiteral("Ⅰ形焊缝"),
             QStringLiteral("单边V形焊缝"), QStringLiteral("U形焊缝"), QStringLiteral("点焊"),
             QStringLiteral("缝焊"), QStringLiteral("塞焊/槽焊") };
}

QString weldTypeName(WeldType t) {
    switch (t) {
        case WeldType::Fillet:   return QStringLiteral("角焊缝");
        case WeldType::VGroove:  return QStringLiteral("V形焊缝");
        case WeldType::IGroove:  return QStringLiteral("Ⅰ形焊缝");
        case WeldType::Bevel:    return QStringLiteral("单边V形焊缝");
        case WeldType::UGroove:  return QStringLiteral("U形焊缝");
        case WeldType::Spot:     return QStringLiteral("点焊");
        case WeldType::Seam:     return QStringLiteral("缝焊");
        case WeldType::Plug:     return QStringLiteral("塞焊/槽焊");
        case WeldType::Count:    break;
    }
    return QStringLiteral("角焊缝");
}

WeldType weldTypeFromName(const QString& name) {
    const QString n = name.trimmed();
    // 英文别名（命令行友好）
    static const QPair<QString, WeldType> kEn[] = {
        { QStringLiteral("fillet"), WeldType::Fillet },
        { QStringLiteral("v"),      WeldType::VGroove },
        { QStringLiteral("vgroove"),WeldType::VGroove },
        { QStringLiteral("i"),      WeldType::IGroove },
        { QStringLiteral("igroove"),WeldType::IGroove },
        { QStringLiteral("bevel"),  WeldType::Bevel },
        { QStringLiteral("u"),      WeldType::UGroove },
        { QStringLiteral("ugroove"),WeldType::UGroove },
        { QStringLiteral("spot"),   WeldType::Spot },
        { QStringLiteral("seam"),   WeldType::Seam },
        { QStringLiteral("plug"),   WeldType::Plug },
    };
    const QString lower = n.toLower();
    for (const auto& p : kEn)
        if (lower == p.first) return p.second;
    // 中文名
    const QStringList names = weldTypeNames();
    for (int i = 0; i < names.size(); ++i)
        if (n == names[i]) return static_cast<WeldType>(i);
    return WeldType::Fillet;
}

// ---------------------------------------------------------------------------
// 尺寸标注文本
// ---------------------------------------------------------------------------
QString weldDimText(const WeldSpec& spec) {
    QString left, right;
    const double k = spec.dims.k, s = spec.dims.s,
                 l = spec.dims.l, e = spec.dims.e;
    const int    n = spec.dims.n;
    // 左侧：尺寸（焊脚 k / 坡口深 s）
    if (spec.type == WeldType::Fillet && k > 0)            left = QStringLiteral("k") + weldFmt(k);
    else if ((spec.type == WeldType::VGroove || spec.type == WeldType::IGroove ||
              spec.type == WeldType::Bevel   || spec.type == WeldType::UGroove ||
              spec.type == WeldType::Plug) && s > 0)       left = QStringLiteral("s") + weldFmt(s);
    else if (k > 0)                                       left = weldFmt(k);
    // 右侧：长度 / 间距 / 段数
    if (l > 0) right += weldFmt(l);
    if (e > 0) right += QStringLiteral(" e") + weldFmt(e);
    if (n > 0) right += QStringLiteral("×") + QString::number(n);
    QString out;
    if (!left.isEmpty())  out += left;
    if (!left.isEmpty() && !right.isEmpty()) out += QStringLiteral("  ");
    if (!right.isEmpty()) out += right;
    return out;
}

// ---------------------------------------------------------------------------
// 几何生成主函数
// ---------------------------------------------------------------------------
std::vector<Element> buildWeldSymbol(const WeldSpec& spec) {
    std::vector<Element> out;
    const double d = std::max(1.0, spec.d);     // 基本符号尺寸，下限 1mm 防退化
    const double w = kWeldLineScale * d;        // 符号线宽 = 2d/3
    const QString layer = spec.layer.isEmpty() ? QStringLiteral("标注") : spec.layer;

    // 局部→世界变换（绕 anchor 旋转 rotationDeg）
    const double a = spec.rotationDeg * M_PI / 180.0;
    const double ca = std::cos(a), sa = std::sin(a);
    auto W = [&](double x, double y) -> Vec3 {
        return Vec3(spec.anchor.x + x * ca - y * sa,
                    spec.anchor.y + x * sa + y * ca, 0.0);
    };

    const double Lref  = 8.0 * d;   // 基准线长度
    const double Llead = 5.0 * d;   // 指引线长度（45°）
    // 膝点(0,0) → 基准线右端 (Lref,0)；指引线尖端 (-Llead,-Llead)
    const Vec3 knee = W(0, 0);
    const Vec3 tip  = W(-Llead, -Llead);

    // ① 基准线（水平）
    out.push_back(makeSeg(knee, W(Lref, 0), w, layer));
    // ② 指引线（膝 → 尖端）
    out.push_back(makeSeg(knee, tip, w, layer));
    // ③ 箭头（尖端处，指向接头）
    {
        Vec3 dir = (tip - knee);
        const double L = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (L > 1e-6) { dir.x /= L; dir.y /= L; }
        // 箭头双翼：自尖端沿 -dir 后退，并左右偏转 25°
        Vec3 back = W(-Llead + d * 0.8 * (-dir.x), -Llead + d * 0.8 * (-dir.y));
        Vec3 perp(-dir.y, dir.x, 0);
        Vec3 w1 = Vec3(back.x + perp.x * d * 0.28, back.y + perp.y * d * 0.28, 0);
        Vec3 w2 = Vec3(back.x - perp.x * d * 0.28, back.y - perp.y * d * 0.28, 0);
        out.push_back(makeSeg(tip, w1, w, layer));
        out.push_back(makeSeg(tip, w2, w, layer));
    }

    // 符号中心（基准线中部）
    const double cx = 4.0 * d;
    // 符号相对基准线的纵向偏移：箭头侧在下方(-)，非箭头侧在上方(+)，双面在线上(0)
    double sy = 0.0;
    if (spec.side == WeldSide::Arrow)      sy = -1.0;   // 下方
    else if (spec.side == WeldSide::Other) sy = +1.0;   // 上方
    // 基本符号几何（attach 在 y=0 处，沿 sy 方向伸展 d）
    auto sym = [&](double x, double y) -> Vec3 { return W(cx + x, sy * y); };
    // 预留：非箭头侧（双面/对称焊缝）的反向映射，当前各基本符号分支仅用 sym()。
    // 保留设计意图但显式标注未使用，避免 -Wall -Wextra 的 -Wunused-but-set-variable。
    [[maybe_unused]] auto symA = [&](double x, double y) -> Vec3 { return W(cx + x, -sy * y); };

    switch (spec.type) {
        case WeldType::Fillet: {  // ▽ 直角三角形（直角在左下，斜边右上）
            std::vector<Vec3> tri = { sym(-d/2, d), sym(d/2, d), sym(-d/2, 0), sym(-d/2, d) };
            out.push_back(makePoly(tri, true, w, layer));
            break;
        }
        case WeldType::VGroove: {  // ∨ V 形开口
            std::vector<Vec3> v = { sym(-d/2, d), sym(0, 0), sym(d/2, d) };
            out.push_back(makePoly(v, false, w, layer));
            break;
        }
        case WeldType::IGroove: {  // ‖ 两条竖平行线
            out.push_back(makeSeg(sym(-d/4, d), sym(-d/4, 0), w, layer));
            out.push_back(makeSeg(sym( d/4, d), sym( d/4, 0), w, layer));
            break;
        }
        case WeldType::Bevel: {    // ∟ 单边 V（弯折线）
            std::vector<Vec3> b = { sym(-d/2, 0), sym(d/2, d) };
            out.push_back(makePoly(b, false, w, layer));
            break;
        }
        case WeldType::UGroove: {  // ∪ U 形（折线近似）
            const double r = d * 0.5;
            std::vector<Vec3> u = {
                sym(-d/2, 0), sym(-d/2, d - r), sym(-d/2 + r, d),
                sym( d/2 - r, d), sym( d/2, d - r), sym( d/2, 0)
            };
            out.push_back(makePoly(u, false, w, layer));
            break;
        }
        case WeldType::Spot: {     // ○ 圆（跨基准线居中）
            out.push_back(makeCircle(W(cx, 0), d * 0.5, w, layer));
            break;
        }
        case WeldType::Seam: {     // ⌐ 两条平行短横线
            out.push_back(makeSeg(sym(-d/2, d), sym(d/2, d), w, layer));
            out.push_back(makeSeg(sym(-d/2, d*0.6), sym(d/2, d*0.6), w, layer));
            break;
        }
        case WeldType::Plug: {     // ⊓ 槽形（∩ 形折线）
            std::vector<Vec3> p = { sym(-d/2, d), sym(-d/2, 0), sym(d/2, 0), sym(d/2, d) };
            out.push_back(makePoly(p, false, w, layer));
            break;
        }
        case WeldType::Count: break;
    }

    // ④ 辅助符号（平面/凸面/凹面）：位于符号同侧更远处的短标记
    if (spec.aux != WeldAux::None) {
        const double off = (sy == 0) ? -1.0 : sy;        // 以符号侧为基准
        const double ay = (spec.aux == WeldAux::Flat) ? 1.4 : (spec.aux == WeldAux::Convex ? 1.4 : 1.4);
        if (spec.aux == WeldAux::Flat) {
            // 平面：一段水平短线
            out.push_back(makeSeg(W(cx - d/2, off * ay * d), W(cx + d/2, off * ay * d), w, layer));
        } else {
            // 凸面/凹面：一段弧（用 5 点折线近似），凸面朝外、凹面朝内
            const bool convex = (spec.aux == WeldAux::Convex);
            std::vector<Vec3> arc;
            const int N = 8;
            for (int i = 0; i <= N; ++i) {
                const double t = -M_PI + (2.0 * M_PI) * i / N;   // -π..π
                const double px = cx + (d/2) * std::cos(t);
                double py = (convex ? 1 : -1) * (1.0 - std::cos(t)) * 0.35 * d + off * ay * d;
                arc.push_back(W(px, py));
            }
            out.push_back(makePoly(arc, false, w, layer));
        }
    }

    // ⑤ 补充符号（周围焊/现场焊/熔透焊）：在膝点处的标记
    if (spec.supp == WeldSupp::Around) {
        out.push_back(makeCircle(knee, d * 0.35, w, layer));     // 周围焊：膝点小圆
    } else if (spec.supp == WeldSupp::MeltThrough) {
        out.push_back(makeCircle(knee, d * 0.2, w, layer));     // 熔透焊：膝点实心点(描边圆)
    } else if (spec.supp == WeldSupp::Field) {
        // 现场焊：膝点处一面小旗（三角形）
        std::vector<Vec3> flag = { knee, W(d*0.4, -d*0.5), W(d*0.4, 0), knee };
        out.push_back(makePoly(flag, true, w, layer));
    }

    // ⑥ 尺寸文本（左侧尺寸 + 右侧长度/间距/段数）
    const QString dim = weldDimText(spec);
    if (!dim.isEmpty()) {
        const double ty = (sy == 0) ? -0.5 : sy * 0.5;   // 文本贴近符号侧
        out.push_back(makeText(W(cx - 3.2 * d, ty * d), dim, d * 0.5, layer));
    }

    // 标识：令整组焊缝符号图元携带 name="焊缝符号"，与粗糙度符号(name="表面粗糙度")
    // 同范式，使合规检查(ComplianceChecker R5)可按 name 聚合识别焊缝符号组。
    const QString tag = QStringLiteral("焊缝符号");
    for (auto& el : out) el.name = tag;

    return out;
}

// ---------------------------------------------------------------------------
// 规范命令字符串（对话框确认时经 CommandInterpreter 执行）
// ---------------------------------------------------------------------------
QString weldCommandString(const WeldSpec& spec) {
    QString cmd = QStringLiteral("WELD ") + weldTypeName(spec.type);
    cmd += QStringLiteral(" %1,%2").arg(spec.anchor.x, 0, 'f', 2).arg(spec.anchor.y, 0, 'f', 2);
    if (spec.dims.k > 0) cmd += QStringLiteral(" k=%1").arg(weldFmt(spec.dims.k));
    if (spec.dims.s > 0) cmd += QStringLiteral(" s=%1").arg(weldFmt(spec.dims.s));
    if (spec.dims.l > 0) cmd += QStringLiteral(" l=%1").arg(weldFmt(spec.dims.l));
    if (spec.dims.e > 0) cmd += QStringLiteral(" e=%1").arg(weldFmt(spec.dims.e));
    if (spec.dims.n > 0) cmd += QStringLiteral(" n=%1").arg(spec.dims.n);
    const char* aux = (spec.aux == WeldAux::Flat)   ? "flat"   :
                      (spec.aux == WeldAux::Convex) ? "convex" :
                      (spec.aux == WeldAux::Concave)? "concave" : "none";
    const char* supp = (spec.supp == WeldSupp::Around)     ? "around"  :
                       (spec.supp == WeldSupp::Field)      ? "field"   :
                       (spec.supp == WeldSupp::MeltThrough)? "melt"    : "none";
    const char* side = (spec.side == WeldSide::Other) ? "other" :
                       (spec.side == WeldSide::Both)  ? "both"  : "arrow";
    cmd += QStringLiteral(" aux=%1 supp=%2 side=%3").arg(QString::fromLatin1(aux),
                                                         QString::fromLatin1(supp),
                                                         QString::fromLatin1(side));
    if (std::fabs(spec.d - 5.0) > 1e-6) cmd += QStringLiteral(" d=%1").arg(weldFmt(spec.d));
    if (std::fabs(spec.rotationDeg) > 1e-6) cmd += QStringLiteral(" rot=%1").arg(weldFmt(spec.rotationDeg));
    return cmd;
}

} // namespace SketchCAD
