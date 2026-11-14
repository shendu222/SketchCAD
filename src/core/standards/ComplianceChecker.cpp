#include "core/standards/ComplianceChecker.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "core/SketchModel.h"   // run 需要遍历图元（elements / layerAt）
#include "core/Layer.h"          // LayerNaming::checkLayerNaming / LayerNamingIssue
#include "core/Element.h"        // ElementType / Element
#include "core/SheetSpec.h"      // currentSheetFormat / standardSheetFrameMargins（R6）

#include <QLineF>
#include <QPointF>

namespace SketchCAD {
namespace {

constexpr double kPi = 3.14159265358979323846;

// ============================================================================
// 标量包围盒：工程制图中水平线/垂直线极常见，其外接矩形宽或高为 0。
// Qt 的 QRectF 把 width==0 || height==0 判为 isEmpty()，而
//   QRectF::united()/intersects()/contains() 对空矩形一律「真空」处理：
//     - united(empty) 返回另一操作数（退化矩形永远长不大）
//     - intersects(empty) 恒为 false（预裁剪永远跳过）
//     - contains(empty) 语义不可靠
// 这会让「两条水平平行线」「超出图框的水平线」全部漏报。
// 故合规判定一律用显式 min/max 标量盒，valid 标志表示「是否吃到过点」，
// 与矩形是否退化解耦。
// ============================================================================
struct BBox {
    bool   valid = false;
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;

    void add(double x, double y) {
        if (!valid) { minX = maxX = x; minY = maxY = y; valid = true; return; }
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    void add(const QPointF& p) { add(p.x(), p.y()); }
    void add(const Vec3& v)    { add(v.x, v.y); }
    void add(const BBox& o) {
        if (!o.valid) return;
        add(o.minX, o.minY);
        add(o.maxX, o.maxY);
    }

    // 两盒在各自方向外扩 pad 后是否相交（用于间隙预裁剪，避免 O(N²) 精确计算）
    bool intersectsPad(const BBox& o, double pad) const {
        return valid && o.valid &&
               maxX + pad >= o.minX && o.maxX + pad >= minX &&
               maxY + pad >= o.minY && o.maxY + pad >= minY;
    }
    // 本盒是否完整落在 outer 内（用于幅面留边，容差抵御浮点误差）
    bool inside(const BBox& outer, double eps = 1e-9) const {
        return valid && outer.valid &&
               minX >= outer.minX - eps && maxX <= outer.maxX + eps &&
               minY >= outer.minY - eps && maxY <= outer.maxY + eps;
    }
    BBox shrunk(double d) const {
        BBox r = *this;
        if (!r.valid) return r;
        r.minX += d; r.minY += d; r.maxX -= d; r.maxY -= d;
        return r;
    }
};

// 是否参与"线宽 / 线段"判定的几何图元（排除文字 / 标注 / 块引用 / 表格等非描边类型）
bool isStrokeType(ElementType t) {
    return t == ElementType::Line      || t == ElementType::Polyline   ||
           t == ElementType::Rectangle || t == ElementType::Circle     ||
           t == ElementType::Arc       || t == ElementType::Spline     ||
           t == ElementType::Ellipse   || t == ElementType::Boundary   ||
           t == ElementType::Xline;
}

// 线段是否近似水平（用于焊缝基准线识别）
bool approxHorizontal(const QLineF& l, double degTol = 12.0) {
    double a = std::fmod(std::abs(l.angle()), 180.0);
    return a <= degTol || a >= 180.0 - degTol;
}

// 把图元展开为线段集合（供两线间隙 / 焊缝基准线识别；圆/弧/椭圆用折线近似）
QVector<QLineF> elementSegments(const Element& e) {
    QVector<QLineF> segs;
    const auto& p = e.pts;
    auto push = [&](const Vec3& a, const Vec3& b) { segs.append(QLineF(a.x, a.y, b.x, b.y)); };
    switch (e.type) {
        case ElementType::Line:
        case ElementType::Xline:
            if (p.size() >= 2) push(p[0], p[1]);
            break;
        case ElementType::Polyline:
        case ElementType::Rectangle:
        case ElementType::Boundary:
        case ElementType::Spline: {   // 样条用控制点折线近似（对间隙判定足够）
            for (size_t i = 1; i < p.size(); ++i) push(p[i - 1], p[i]);
            if (e.closed && p.size() >= 2) push(p.back(), p.front());
            break;
        }
        case ElementType::Arc:
        case ElementType::Circle:
        case ElementType::Ellipse: {
            const Vec3 c = p.empty() ? Vec3(0, 0, 0) : p[0];
            const double r  = e.radius;
            const double rY = (e.type == ElementType::Ellipse) ? e.radiusY : r;
            const double a0 = (e.type == ElementType::Circle) ? 0.0 : e.startAngle * kPi / 180.0;
            const double a1 = (e.type == ElementType::Circle) ? 2.0 * kPi : e.endAngle * kPi / 180.0;
            const int N = 24;
            auto ptAt = [&](double ang) -> Vec3 {
                if (e.type == ElementType::Ellipse)
                    return Vec3(c.x + r * std::cos(ang), c.y + rY * std::sin(ang), 0);
                return Vec3(c.x + r * std::cos(ang), c.y + r * std::sin(ang), 0);
            };
            Vec3 prev = ptAt(a0);
            for (int i = 1; i <= N; ++i) {
                const double ang = a0 + (a1 - a0) * double(i) / double(N);
                const Vec3 q = ptAt(ang);
                push(prev, q);
                prev = q;
            }
            break;
        }
        default: break;
    }
    return segs;
}

// 元素包围盒（标量版；不依赖 SketchModel::boundsOf 私有接口；
// 圆/弧/椭圆按整圆估算，足够合规判定）。退化（水平/垂直线）仍为 valid。
BBox elementBBox(const Element& e) {
    BBox b;
    if (e.pts.empty()) return b;
    const auto& p = e.pts;
    switch (e.type) {
        case ElementType::Circle:
        case ElementType::Arc: {
            const double r = e.radius;
            b.add(p[0].x - r, p[0].y - r);
            b.add(p[0].x + r, p[0].y + r);
            break;
        }
        case ElementType::Ellipse: {
            const double r = e.radius, ry = e.radiusY;
            b.add(p[0].x - r, p[0].y - ry);
            b.add(p[0].x + r, p[0].y + ry);
            break;
        }
        default:
            for (const Vec3& v : p) b.add(v);
            break;
    }
    return b;
}

// 两线段最小距离（端点-端点 + 端点-线段，足够间隙判定）
double segSegDist(const QLineF& a, const QLineF& b) {
    double d = QLineF(a.p1(), b.p1()).length();
    d = std::min(d, QLineF(a.p1(), b.p2()).length());
    d = std::min(d, QLineF(a.p2(), b.p1()).length());
    d = std::min(d, QLineF(a.p2(), b.p2()).length());
    auto ptSeg = [](const QPointF& p, const QLineF& s) -> double {
        const double l2 = s.length() * s.length();
        if (l2 < 1e-12) return QLineF(p, s.p1()).length();
        double t = ((p.x() - s.p1().x()) * (s.p2().x() - s.p1().x()) +
                    (p.y() - s.p1().y()) * (s.p2().y() - s.p1().y())) / l2;
        t = std::max(0.0, std::min(1.0, t));
        const QPointF proj(s.p1().x() + t * (s.p2().x() - s.p1().x()),
                           s.p1().y() + t * (s.p2().y() - s.p1().y()));
        return QLineF(p, proj).length();
    };
    d = std::min(d, ptSeg(a.p1(), b));
    d = std::min(d, ptSeg(a.p2(), b));
    d = std::min(d, ptSeg(b.p1(), a));
    d = std::min(d, ptSeg(b.p2(), a));
    return d;
}

// 按 name 标签就近聚类（同一符号的各图元间距在 radius 内归入同一簇）
template <typename F>
void clusterByName(const SketchModel* m, const QString& tag, double radius,
                   F perCluster) {
    QVector<Element> sym;
    for (const Element& e : m->elements())
        if (e.name.compare(tag, Qt::CaseInsensitive) == 0) sym.append(e);
    if (sym.isEmpty()) return;
    struct Cluster { QVector<Element> es; BBox bb; };
    QVector<Cluster> cs;
    for (const Element& e : sym) {
        const BBox b = elementBBox(e);
        bool placed = false;
        for (Cluster& c : cs) {
            if (c.bb.intersectsPad(b, radius)) {
                c.es.append(e);
                c.bb.add(b);
                placed = true;
                break;
            }
        }
        if (!placed) {
            Cluster c;
            c.es.append(e);
            c.bb.add(b);
            cs.append(c);
        }
    }
    for (const Cluster& c : cs) perCluster(c);
}

// ---- R1 线宽粗细比（应为 2:1，GB/T 14665 表1）----
void checkLineWidthRatio(const SketchModel* m, QVector<ComplianceIssue>& out) {
    QMap<double, int> firstId;       // 线宽 → 首个图元 id
    QVector<double> widths;
    for (const Element& e : m->elements()) {
        if (!isStrokeType(e.type)) continue;
        const double w = e.effectiveLineWidth();
        if (w <= 0) continue;
        if (!firstId.contains(w)) firstId[w] = e.id;
        widths.append(w);
    }
    if (widths.size() < 2) return;   // 单一线宽不构成粗细比，放行
    QSet<double> du;
    for (double w : widths) du.insert(w);
    QVector<double> dw(du.begin(), du.end());
    std::sort(dw.begin(), dw.end(), std::greater<double>());
    // 相邻线宽对必须维持 ≈2:1（可容纳多组 2:1 级联，如 2.0/1.0/0.5）
    for (int i = 0; i + 1 < dw.size(); ++i) {
        const double w1 = dw[i], w2 = dw[i + 1];
        const double ratio = w1 / w2;
        if (ratio < 1.85 || ratio > 2.15) {
            ComplianceIssue iss;
            iss.elementId = firstId.value(w2, 0);
            iss.ruleId    = QStringLiteral("R1_line_width_ratio");
            iss.severity  = QStringLiteral("警告");
            // Qt QString::arg 只认 %1/%2/... 序号占位符；printf 风格 %.2f 不会被替换，
            // 会让日志/面板/AI 工具结果残留字面量且丢失实测比值。
            iss.message   = QStringLiteral("粗线(%1mm)与细线(%2mm)的线宽比为 %3:1，不符合 GB/T 14665 规定的 2:1（粗:细）。")
                                .arg(w1, 0, 'f', 2).arg(w2, 0, 'f', 2).arg(ratio, 0, 'f', 2);
            iss.fixHint   = QStringLiteral("将细线线宽调整为粗线的一半（如粗 1.0→细 0.5、粗 1.4→细 0.7），使粗细比恰为 2:1。");
            out.append(iss);
        }
    }
}

// ---- R2 两线最小间隙 ≥ 0.7mm（GB/T 14665）----
void checkMinGap(const SketchModel* m, QVector<ComplianceIssue>& out) {
    struct Item { int id; QVector<QLineF> segs; BBox bbox; };
    QVector<Item> items;
    for (const Element& e : m->elements()) {
        if (!isStrokeType(e.type)) continue;
        QVector<QLineF> segs = elementSegments(e);
        if (segs.isEmpty()) continue;
        BBox bb;
        for (const QLineF& s : segs) { bb.add(s.p1()); bb.add(s.p2()); }
        items.append({e.id, segs, bb});
    }
    const double minGap = 0.7;       // mm
    for (int i = 0; i < items.size(); ++i) {
        for (int j = i + 1; j < items.size(); ++j) {
            // 标量盒预裁剪：两盒间距已 ≥ minGap 则无需精确计算（退化盒同样有效）
            if (!items[i].bbox.intersectsPad(items[j].bbox, minGap))
                continue;
            double best = 1e9;
            for (const QLineF& sa : items[i].segs) {
                for (const QLineF& sb : items[j].segs) {
                    const double d = segSegDist(sa, sb);
                    if (d < best) best = d;
                    if (best < minGap) break;
                }
                if (best < minGap) break;
            }
            if (best < minGap && best > 1e-3) {   // 排除重合/共端点的线段
                ComplianceIssue iss;
                iss.elementId = items[j].id;
                iss.ruleId    = QStringLiteral("R2_min_line_gap");
                iss.severity  = QStringLiteral("警告");
                iss.message   = QStringLiteral("图元 #%1 与 #%2 的最小间隙为 %3mm，小于 GB/T 14665 规定的两线最小间距 0.7mm。")
                                    .arg(items[i].id).arg(items[j].id).arg(best, 0, 'f', 3);
                iss.fixHint   = QStringLiteral("增大两线间距至 ≥0.7mm，或将其中一条改到其它图层以避免过近重叠。");
                out.append(iss);
                break;   // 该元素只报一次
            }
        }
    }
}

// ---- R3 字号下限（A2~A4 字母数字 ≥ 3.5mm，GB/T 14665 表3）----
void checkTextHeight(const SketchModel* m, QVector<ComplianceIssue>& out) {
    const double minH = 3.5;   // mm
    for (const Element& e : m->elements()) {
        double h = 0;
        bool check = false;
        switch (e.type) {
            case ElementType::Text:
            case ElementType::MText:
                if (e.radius > 0) { h = e.radius; check = true; }   // radius==0 表示沿用默认字高(3.5)，放行
                break;
            case ElementType::Dimension:
                if (e.dimTextHeight > 0) { h = e.dimTextHeight; check = true; }
                break;
            case ElementType::MLeader:
            case ElementType::Fcf:
            case ElementType::Table:
                if (e.radius > 0) { h = e.radius; check = true; }
                else if (e.dimTextHeight > 0) { h = e.dimTextHeight; check = true; }
                break;
            default: break;
        }
        if (!check) continue;
        if (h < minH) {
            ComplianceIssue iss;
            iss.elementId = e.id;
            iss.ruleId    = QStringLiteral("R3_text_height");
            iss.severity  = QStringLiteral("警告");
            iss.message   = QStringLiteral("图元 #%1 字号 %2mm 低于 GB/T 14665 下限 %3mm（A2~A4 字母数字）。")
                                .arg(e.id).arg(h, 0, 'f', 2).arg(minH, 0, 'f', 1);
            iss.fixHint   = QStringLiteral("将字号提高到 ≥3.5mm（A0/A1 建议 ≥5mm）。");
            out.append(iss);
        }
    }
}

// ---- R4 粗糙度符号书写规范（须注写 Ra 数值，GB/T 131-2006）----
void checkRoughness(const SketchModel* m, QVector<ComplianceIssue>& out) {
    clusterByName(m, QStringLiteral("表面粗糙度"), 15.0, [&](const auto& c) {
        bool hasValue = false;
        for (const Element& e : c.es) {
            if (e.type == ElementType::Text && !e.text.isEmpty()) {
                for (const QChar ch : e.text)
                    if (ch.isDigit()) { hasValue = true; break; }
            }
        }
        if (!hasValue) {
            ComplianceIssue iss;
            iss.elementId = c.es.first().id;
            iss.ruleId    = QStringLiteral("R4_roughness_notation");
            iss.severity  = QStringLiteral("提示");
            iss.message   = QStringLiteral("表面粗糙度符号（图元 #%1）缺少 Ra 数值标注，不符合 GB/T 131-2006 书写规范。")
                                .arg(c.es.first().id);
            iss.fixHint   = QStringLiteral("为粗糙度符号补注 Ra 值（如 Ra 1.6、Rz 6.3），数值位于符号长边横线上方。");
            out.append(iss);
        }
    });
}

// ---- R5 焊缝符号完整性（基准线 + 基本符号，GB/T 324）----
void checkWeld(const SketchModel* m, QVector<ComplianceIssue>& out) {
    clusterByName(m, QStringLiteral("焊缝符号"), 20.0, [&](const auto& c) {
        bool hasRefLine = false, hasSymbol = false;
        for (const Element& e : c.es) {
            const QVector<QLineF> segs = elementSegments(e);
            for (const QLineF& s : segs) if (approxHorizontal(s)) hasRefLine = true;
            if (e.type == ElementType::Polyline || e.type == ElementType::Circle) hasSymbol = true;
        }
        if (!hasRefLine || !hasSymbol) {
            ComplianceIssue iss;
            iss.elementId = c.es.first().id;
            iss.ruleId    = QStringLiteral("R5_weld_symbol");
            iss.severity  = QStringLiteral("提示");
            iss.message   = QStringLiteral("焊缝符号（图元 #%1）不完整：缺少%s。")
                                .arg(c.es.first().id)
                                .arg(!hasRefLine ? QStringLiteral("基准线") : QStringLiteral("基本符号"));
            iss.fixHint   = QStringLiteral("按 GB/T 324 补全焊缝符号：基准线 + 基本符号（必要时加指引线、箭头与尺寸）。");
            out.append(iss);
        }
    });
}

// ---- R6 幅面留边（内容不得超出图框留边区，GB/T 14689）----
void checkSheetMargin(const SketchModel* m, QVector<ComplianceIssue>& out) {
    // 依据"图框"层图元推断图纸外边界；无图框则无法判定，跳过以避免误报
    BBox paper;
    for (const Element& e : m->elements())
        if (e.layer == QStringLiteral("图框")) paper.add(elementBBox(e));
    if (!paper.valid) return;
    SheetFormat fmt = currentSheetFormat();
    const double mg = standardSheetFrameMargins(fmt).c;   // 装订边外的留边 c
    const double margin = (mg > 0) ? mg : 5.0;            // 回落保守值
    const BBox frame = paper.shrunk(margin);
    if (!frame.valid || frame.minX > frame.maxX || frame.minY > frame.maxY) return;
    for (const Element& e : m->elements()) {
        if (e.layer == QStringLiteral("图框")) continue;
        const BBox b = elementBBox(e);
        if (!b.valid) continue;
        if (!b.inside(frame)) {     // 退化盒（水平/垂直线）同样参与判定
            ComplianceIssue iss;
            iss.elementId = e.id;
            iss.ruleId    = QStringLiteral("R6_sheet_margin");
            iss.severity  = QStringLiteral("提示");
            iss.message   = QStringLiteral("图元 #%1 超出图框留边区，不符合 GB/T 14689 幅面留边要求。").arg(e.id);
            iss.fixHint   = QStringLiteral("将内容移入图框内，或按内容重新生成更大图幅（sheetframe）的图框。");
            out.append(iss);
        }
    }
}

// ---- R7 图层命名（GB/T 18617 / ISO 13567）----
void checkLayerNamingRule(const SketchModel* m, QVector<ComplianceIssue>& out) {
    QStringList names;
    for (int i = 0; i < m->layerCount(); ++i) names.append(m->layerAt(i).name);
    const QVector<LayerNamingIssue> raw = LayerNaming::checkLayerNaming(names);
    for (const LayerNamingIssue& r : raw) {
        long firstId = 0;   // 找该层首个图元 id，便于面板跳转
        for (const Element& e : m->elements())
            if (e.layer == r.layerName) { firstId = e.id; break; }
        ComplianceIssue iss;
        iss.elementId = firstId;
        iss.ruleId    = QStringLiteral("R7_layer_naming");
        iss.severity  = QStringLiteral("警告");
        iss.message   = QStringLiteral("图层「%1」命名不符合 GB/T 18617/ISO 13567：%2").arg(r.layerName, r.reason);
        iss.fixHint   = QStringLiteral("按 GB/T 18617 重命名图层，如「01_轮廓线」「04_中心线」（保留默认层 0）。");
        out.append(iss);
    }
}

// ---- 命令 → 面板刷新回调桥（全局单例 sink）----
ComplianceChecker::ComplianceReportSink& sinkRef() {
    static ComplianceChecker::ComplianceReportSink s;
    return s;
}

} // namespace

// 仅图层命名合规检查（结构化 Issue 列表）
QVector<ComplianceIssue> ComplianceChecker::checkLayerNaming(const QStringList& layerNames) {
    QVector<ComplianceIssue> out;
    const QVector<LayerNamingIssue> raw = LayerNaming::checkLayerNaming(layerNames);
    for (const LayerNamingIssue& r : raw) {
        ComplianceIssue iss;
        iss.elementId = 0;
        iss.ruleId    = QStringLiteral("R7_layer_naming");
        iss.severity  = QStringLiteral("警告");
        iss.message   = QStringLiteral("图层「%1」命名不符合 GB/T 18617/ISO 13567：%2").arg(r.layerName, r.reason);
        iss.fixHint   = QStringLiteral("按 GB/T 18617 重命名图层，如「01_轮廓线」（保留默认层 0）。");
        out.append(iss);
    }
    return out;
}

// 七规则完整检查入口
QVector<ComplianceIssue> ComplianceChecker::run(const SketchModel* model) {
    QVector<ComplianceIssue> out;
    if (!model) return out;
    checkLineWidthRatio(model, out);
    checkMinGap(model, out);
    checkTextHeight(model, out);
    checkRoughness(model, out);
    checkWeld(model, out);
    checkSheetMargin(model, out);
    checkLayerNamingRule(model, out);
    return out;
}

void ComplianceChecker::setComplianceReportSink(ComplianceReportSink sink) {
    sinkRef() = std::move(sink);
}

void ComplianceChecker::notifyComplianceReport(const QVector<ComplianceIssue>& issues) {
    if (sinkRef()) sinkRef()(issues);
}

} // namespace SketchCAD
