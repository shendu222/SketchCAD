#include "Gdt.h"

#include <algorithm>
#include <cmath>

namespace SketchCAD {

// ============================================================================
// 形位公差符号（GB/T 1182 子集）
// ============================================================================
int fcfSymbolCount() { return (int)FcfSymbol::TotalRunout + 1; }

const char* fcfSymbolName(FcfSymbol s) {
    switch (s) {
        case FcfSymbol::Straightness:    return "直线度";
        case FcfSymbol::Flatness:        return "平面度";
        case FcfSymbol::Circularity:     return "圆度";
        case FcfSymbol::Cylindricity:    return "圆柱度";
        case FcfSymbol::Parallelism:     return "平行度";
        case FcfSymbol::Perpendicularity:return "垂直度";
        case FcfSymbol::Angularity:      return "倾斜度";
        case FcfSymbol::Position:        return "位置度";
        case FcfSymbol::Symmetry:        return "对称度";
        case FcfSymbol::CircularRunout:  return "圆跳动";
        case FcfSymbol::TotalRunout:     return "全跳动";
    }
    return "未知";
}

bool fcfSymbolValid(int v) { return v >= 0 && v < fcfSymbolCount(); }

namespace {

// 把圆离散成线段（供绘制/包围盒/拾取统一按 Seg 处理，避免各处再写一遍圆弧近似）
void appendCircle(std::vector<geo::Seg>& out, const Vec3& c, double r, int seg = 24) {
    if (r <= 1e-9) return;
    seg = std::max(8, seg);
    for (int i = 0; i < seg; ++i) {
        const double a0 = 2.0 * M_PI * i / seg;
        const double a1 = 2.0 * M_PI * (i + 1) / seg;
        out.push_back(geo::Seg{ Vec3(c.x + r * std::cos(a0), c.y + r * std::sin(a0), c.z),
                                Vec3(c.x + r * std::cos(a1), c.y + r * std::sin(a1), c.z) });
    }
}

// 直线段辅助
geo::Seg seg(double x0, double y0, double x1, double y1, double z = 0.0) {
    return geo::Seg{ Vec3(x0, y0, z), Vec3(x1, y1, z) };
}

// 在以 c 为中心、半边长 h 的方框内绘制几何特征符号（GB/T 1182）。
// 符号本身与框格高无关，只按 h 缩放，故各符号统一在此按单位方框定义。
void appendFcfSymbolStrokes(std::vector<geo::Seg>& out, FcfSymbol sym, const Vec3& c, double h) {
    const double z = c.z;
    // 归一化坐标：u,v ∈ [-h/2, h/2]；各符号用基础图元组合。
    const double L = -h / 2.0, R = h / 2.0, M = 0.0;   // 左/右/中
    switch (sym) {
        case FcfSymbol::Straightness:   // ─ 直线度：一条水平线
            out.push_back(seg(L, M, R, M, z));
            break;
        case FcfSymbol::Flatness:       // ▱ 平面度：平行四边形
            out.push_back(seg(L + h*0.12, M - h*0.22, R - h*0.12, M - h*0.22, z));
            out.push_back(seg(L + h*0.12, M + h*0.22, R - h*0.12, M + h*0.22, z));
            out.push_back(seg(L + h*0.12, M - h*0.22, L + h*0.12, M + h*0.22, z));
            out.push_back(seg(R - h*0.12, M - h*0.22, R - h*0.12, M + h*0.22, z));
            break;
        case FcfSymbol::Circularity:    // ○ 圆度：一个圆
            appendCircle(out, c, h * 0.30, 24);
            break;
        case FcfSymbol::Cylindricity:   // 圆柱度：圆 + 上下两平行线
            out.push_back(seg(L, M + h*0.30, R, M + h*0.30, z));
            out.push_back(seg(L, M - h*0.30, R, M - h*0.30, z));
            appendCircle(out, c, h * 0.30, 24);
            break;
        case FcfSymbol::Parallelism:    // ╱╱ 平行度：两条平行斜线
            out.push_back(seg(L + h*0.05, M - h*0.26, L + h*0.30, M + h*0.26, z));
            out.push_back(seg(R - h*0.30, M - h*0.26, R - h*0.05, M + h*0.26, z));
            break;
        case FcfSymbol::Perpendicularity: // ⊥ 垂直度：竖线立于横线
            out.push_back(seg(L, M - h*0.26, R, M - h*0.26, z));
            out.push_back(seg(M, M - h*0.26, M, M + h*0.30, z));
            break;
        case FcfSymbol::Angularity:     // ∠ 倾斜度：一个角
            out.push_back(seg(L, M - h*0.26, R, M - h*0.26, z));
            out.push_back(seg(L + h*0.18, M - h*0.26, R, M + h*0.30, z));
            break;
        case FcfSymbol::Position:       // ⊕ 位置度：圆 + 十字
            appendCircle(out, c, h * 0.32, 24);
            out.push_back(seg(L + h*0.06, M, R - h*0.06, M, z));
            out.push_back(seg(M, M - h*0.32, M, M + h*0.32, z));
            break;
        case FcfSymbol::Symmetry:       // 对称度：三条平行短线
            out.push_back(seg(L + h*0.10, M + h*0.26, R - h*0.10, M + h*0.26, z));
            out.push_back(seg(L + h*0.10, M,         R - h*0.10, M,         z));
            out.push_back(seg(L + h*0.10, M - h*0.26, R - h*0.10, M - h*0.26, z));
            break;
        case FcfSymbol::CircularRunout: // ↗ 圆跳动：单箭头
            out.push_back(seg(L + h*0.10, M - h*0.28, R - h*0.10, M + h*0.30, z));
            break;
        case FcfSymbol::TotalRunout:    // ⇈ 全跳动：双箭头
            out.push_back(seg(L + h*0.14, M - h*0.28, R - h*0.14, M + h*0.30, z));
            out.push_back(seg(L + h*0.36, M - h*0.28, R - h*0.08, M + h*0.30, z));
            break;
    }
}

// 估算一行文字的宽度(mm)。核心层不引 QFontMetrics（离屏无字体环境会退化），
// 用「字符数 × 字高 × 系数」的确定性估算，保证布局在测试与实机一致。
double estTextWidth(const QString& s, double h) {
    if (s.isEmpty()) return 0.0;
    return s.size() * h * 0.62;
}

} // namespace

// ============================================================================
// 多重引线
// ============================================================================
MLeaderGeom buildMLeader(const Element& e, double dimScale) {
    MLeaderGeom g;
    const double ds = (dimScale > 1e-9) ? dimScale : 1.0;
    if (e.pts.size() < 2) return g;              // 至少要有附着点 + 文字起点
    g.valid = true;

    const Vec3 p0 = e.pts[0];                    // 箭头附着点（尖端）
    const bool hasElbow = e.pts.size() >= 3;
    const Vec3 p1 = e.pts[1];                    // 折点（hasElbow）或文字基线起点
    const Vec3 p2 = hasElbow ? e.pts[2] : p1;    // 文字基线起点

    // 箭头（实心三角：尖端 p0，底边中点沿引线方向后退 arrowLen）
    g.arrowTip = p0;
    g.arrowLen   = (e.mleaderArrowLen > 1e-6) ? e.mleaderArrowLen : 3.5;
    g.arrowWidth = g.arrowLen * 0.31;            // ≈ 2d 宽
    Vec3 dir = p1 - p0;
    double len = std::hypot(dir.x, dir.y);
    if (len < 1e-9) { dir = Vec3(1, 0, 0); len = 1.0; }
    dir.x /= len; dir.y /= len;
    // 引线折线：附着点 → 折点（去掉箭头占据的那一小段，视觉上箭头独立）
    g.leader.push_back(geo::Seg{ p0, p1 });
    if (hasElbow) g.leader.push_back(geo::Seg{ p1, p2 });

    // 基线（水平段，长度由 mleaderLanding 决定；<=0 无基线）
    const double landingLen = e.mleaderLanding;
    if (landingLen > 1e-9) {
        // 基线自文字起点沿 +X 水平伸出（GB：基线与文字左端平齐，箭头侧在左）
        g.landing.push_back(geo::Seg{ p2, Vec3(p2.x + landingLen, p2.y, p2.z) });
    }

    // 多行文字：按 \n 分行，自文字起点向下排（世界 Y 向上，故逐行 y 递减）
    const double th = (e.radius > 1e-6) ? e.radius / ds : 3.5;
    const QStringList lines = e.text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        GdtText t;
        t.text   = lines[i];
        t.height = th;
        t.anchor = Vec3(p2.x, p2.y - th * (i + 1) * 1.3, p2.z);   // 首行基线在 p2 之下
        g.texts.push_back(std::move(t));
    }
    return g;
}

// ============================================================================
// 形位公差框格
// ============================================================================
FcfGeom buildFcf(const Element& e, double dimScale) {
    FcfGeom g;
    const double ds = (dimScale > 1e-9) ? dimScale : 1.0;
    const double h = (e.radius > 1e-6) ? e.radius : 7.0;    // 框格高/字高
    g.height = h;
    const Vec3 anchor = e.pts.empty() ? Vec3(0, 0, 0) : e.pts[0];
    g.valid = true;

    // ---- 组装各格内容（顺序：符号 | 修饰符 | 公差值 | 基准…）----
    struct CellSpec { QString text; bool isSymbol; };
    std::vector<CellSpec> specs;
    specs.push_back(CellSpec{ QString(), true });                       // 符号格（必选）
    if (!e.fcfModifier.isEmpty())
        specs.push_back(CellSpec{ e.fcfModifier, false });
    if (!e.fcfTolerance.isEmpty())
        specs.push_back(CellSpec{ e.fcfTolerance, false });
    for (const auto& d : e.fcfDatums)
        if (!d.isEmpty()) specs.push_back(CellSpec{ d, false });

    // ---- 计算各格宽度并布局（自左向右，锚点=左下角，向上为正）----
    const double pad = h * 0.35;          // 文字格左右留白
    double x = anchor.x;
    for (size_t i = 0; i < specs.size(); ++i) {
        const double w = specs[i].isSymbol ? h
                                           : std::max(h * 0.6, estTextWidth(specs[i].text, h) + pad * 2.0);
        FcfCell c;
        c.rect = QRectF(QPointF(x, anchor.y), QPointF(x + w, anchor.y + h));
        c.text = specs[i].text;
        c.isSymbol = specs[i].isSymbol;
        g.cells.push_back(c);

        if (!c.text.isEmpty()) {
            GdtText t;
            t.text   = c.text;
            t.height = h * 0.62;           // 格内字高（GB：小于框格高）
            t.anchor = Vec3(x + w / 2.0, anchor.y + h / 2.0 - t.height * 0.35, anchor.z);
            g.texts.push_back(std::move(t));
        }
        x += w;
    }
    const double totalW = x - anchor.x;
    if (totalW <= 1e-9) return g;

    // ---- 框线：外框 + 各竖分隔线 ----
    const double y0 = anchor.y, y1 = anchor.y + h, z = anchor.z;
    g.frame.push_back(seg(anchor.x, y0, anchor.x + totalW, y0, z));
    g.frame.push_back(seg(anchor.x + totalW, y0, anchor.x + totalW, y1, z));
    g.frame.push_back(seg(anchor.x + totalW, y1, anchor.x, y1, z));
    g.frame.push_back(seg(anchor.x, y1, anchor.x, y0, z));
    for (size_t i = 1; i < g.cells.size(); ++i) {
        const double cx = g.cells[i].rect.left();
        g.frame.push_back(seg(cx, y0, cx, y1, z));
    }

    // ---- 符号笔画（居中于符号格）----
    if (fcfSymbolValid(e.fcfSymbol) && !g.cells.empty()) {
        const FcfCell& sc = g.cells[0];
        const Vec3 c(sc.rect.center().x(), sc.rect.center().y(), z);
        appendFcfSymbolStrokes(g.symbolStrokes, (FcfSymbol)e.fcfSymbol, c, h * 0.8);
    }
    (void)ds;
    return g;
}

} // namespace SketchCAD
