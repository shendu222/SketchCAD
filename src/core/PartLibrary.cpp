#include "PartLibrary.h"
#include "InvoluteGear.h"
#include "GbFastenerTable.h"
#include "LineStyleConfig.h"   // 细档线宽（螺纹小径细实线）
#include "Geometry.h"          // geo::measureDimension（标注实测值）
#include "Block.h"        // 出库：makeDefFromEntities / makeRef / AttributeDef
#include <cmath>

namespace SketchCAD {

// ------------------------------------------------------------
// 图元构造辅助（紧固件侧视/正视简化几何，均为闭合 Polyline 或 Circle）
// ------------------------------------------------------------
namespace {

constexpr double K_PI = 3.14159265358979323846;

Element mkPolyline(const std::vector<Vec3>& pts, const QString& name, bool closed = true) {
    Element e;
    e.type = ElementType::Polyline;
    e.pts = pts;
    e.closed = closed;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = true;
    return e;
}
Element mkCircle(const Vec3& c, double r, const QString& name) {
    Element e;
    e.type = ElementType::Circle;
    e.pts = { c };
    e.radius = r;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = true;
    return e;
}
Element mkLine(const Vec3& a, const Vec3& b, const QString& name,
               LineStyle st = LineStyle::Solid) {
    Element e;
    e.type = ElementType::Line;
    e.pts = { a, b };
    e.closed = false;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = st;
    e.lineWidthAuto = true;
    return e;
}
Element mkArc(const Vec3& c, double r, double a0Deg, double a1Deg, const QString& name) {
    Element e;
    e.type = ElementType::Arc;
    e.pts = { c };
    e.radius = r;
    e.startAngle = a0Deg;
    e.endAngle = a1Deg;
    e.layer = QStringLiteral("0");
    e.name = name;
    e.color = QColor(26, 29, 40);
    e.lineStyle = LineStyle::Solid;
    e.lineWidthAuto = true;
    return e;
}
// 细实线（GB/T 14665 细线档）：Solid 样式 + 手动细档线宽（随当前线宽组，与点划线同档）。
void mkThin(Element& e) {
    e.lineWidthAuto = false;
    e.lineWidth = defaultWidthForStyle(LineStyle::DashDot);
}
// 多行文字（局部放大图标识 / 技术要求等）：pts[0]=锚点、radius=字高(mm)、textAlign=9 点对齐。
Element mkMText(const Vec3& at, const QString& txt, double hMm, int align, const QString& name) {
    Element e;
    e.type = ElementType::MText;
    e.pts = { at };
    e.radius = hMm;
    e.text = txt;
    e.textAlign = align;
    e.name = name;
    e.layer = QStringLiteral("0");
    e.color = QColor(26, 29, 40);
    e.lineWidthAuto = true;
    return e;
}
// 尺寸/引线标注：线性 pts={测点1,测点2[,尺寸线位置]}；角度 {顶点,边1,边2}(radius=尺寸弧半径)；
// 引线 {箭头,折点}+text。text 非空 = 显示覆盖文本（附图字母标注格式：k/l/(b)/β/…）。
Element mkDim(DimensionType dt, std::vector<Vec3> pts, const QString& txt) {
    Element e;
    e.type = ElementType::Dimension;
    e.dtype = dt;
    e.pts = std::move(pts);
    e.text = txt;
    e.name = QStringLiteral("尺寸标注");
    e.layer = QStringLiteral("0");
    e.color = QColor(26, 29, 40);
    e.dimValue = geo::measureDimension(e);
    return e;
}

// 正六边形（点朝 +X）：外接圆半径 R，跨对边(垂直方向 y 向) = R√3。
// 使六边形 y 向高度 = acrossFlats → R = s/√3。
std::vector<Vec3> hexPts(const Vec3& center, double acrossFlats) {
    const double R = acrossFlats / std::sqrt(3.0);
    std::vector<Vec3> p;
    p.reserve(6);
    for (int i = 0; i < 6; ++i) {
        const double a = (60.0 * i) * K_PI / 180.0;   // 0,60,...,300（顶点在 ±X）
        p.push_back(Vec3(center.x + R * std::cos(a), center.y + R * std::sin(a), 0.0));
    }
    return p;
}

// 内建零件生成器 ------------------------------------------------

// 齿轮（P4）：参数 teeth / module / pressureAngle → 闭合 Polyline 齿廓
class GearGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("gear"); }
    QString displayName() const override { return QStringLiteral("渐开线齿轮"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* = nullptr) const override {
        const int teeth = params.value(QStringLiteral("teeth"), 20).toInt();
        const double module = params.value(QStringLiteral("module"), 2.0).toDouble();
        const double pa = params.value(QStringLiteral("pressureAngle"), 20.0).toDouble();
        std::vector<Vec3> pts;
        if (!InvoluteGear::profile(teeth, module, pa, pts, true)) return false;
        out.push_back(mkPolyline(pts, QStringLiteral("齿轮")));
        return true;
    }
};

// 六角头螺栓（GB/T 5782 标注图复刻，68 图元，水平置放、头左杆右）：
//   主视（30）：六角头侧视（对角 e 轮廓）+ 头部左端三段倒角弧（高度四等分：上/下 1/4 侧棱面
//     截交弧 + 中 1/2 正棱面截交弧，圆锥截交双曲线以圆弧代替）+ 半高处棱线 + 头下过渡圆角 r
//     + 光杆/螺纹大径粗实线 + 不完整螺纹波浪过渡线 + 小径细实线 ×2 + 末端 30° 倒角 c·tan30°
//     + 螺纹根部线 ×8（颈部少量，细实线）+ 双点划线轴线 + 局部放大圈 X + 标注 10（真实数值）
//     + 根部线标注 ×1（在图中说明已绘根线数量）；
//   端面（8）：正六边形（对边 s 水平）+ 倒角内切圆（r=s/2 与六边相切）+ 大径整圆
//     + 小径 3/4 细弧 + 双点划线十字中心线 + 标注 2（真实数值）；
//   局部放大图 X（19，2:1）：三段倒角弧 + 头下过渡圆角 r + 杆部 + 两侧波浪折断线
//     + 双点划线轴线 + 标注 6（真实数值/R 值）+ 视图名；
//   技术要求（1）：MText，置于放大图下方空白区（不与图形重叠）。
// 尺寸来源：GbFastenerTable（s/k/d/e，G3 禁改）；派生（GB/T 5782 参考规则，须人工校核）：
//   b=min(2d+6,杆长)、lg=l−b（夹紧长）、u=min(lg,0.35d)≈2P（不完整螺纹）、ls=lg−u（光杆）、
//   c=0.15d（30° 倒角半角）、d1=0.85d、r=max(0.25,0.05d)（头下圆角）、dw=0.9s（支承面）、da≈e。
// 三段弧几何（30° 倒角锥截交，xd=√3·Δ、Δ=(e−s)/2）：
//   中弧：过 (xd,±e/4) 与端面 (0,0)，圆心 (ρm,0)，ρm=(xd²+(e/4)²)/(2xd)；
//   侧弧：过 (xd,±e/4) 与 (xd,±e/2)、左凸至 (0,±3e/8)，圆心 (cx,±3e/8)，cx=半径=(xd²+(e/8)²)/(2xd)。
// 标注文本全部为真实数值（fmt：一位小数、整数去尾），角度/半径用 30°、R0.3、0.8×30° 形式。
class BoltGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("bolt"); }
    QString displayName() const override { return QStringLiteral("六角头螺栓"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const double length  = params.value(QStringLiteral("length"), 30.0).toDouble();
        if (nominal <= 0.0 || length <= 0.0) return false;
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::HexBolt, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("六角头螺栓：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        const double s = sp.acrossFlats;   // 对边宽 s
        const double k = sp.thickness;     // 头高 k（轴向）
        const double d = sp.innerD;        // 螺纹大径（表内简化≈公称）
        const double e = sp.outerD;        // 对角 e（头部外接圆直径）
        // ---- 布局：主视原点=头部外端面与轴线交点，+X 指向杆端 ----
        const double R    = e / 2.0;
        const double dEl  = (e - s) / 2.0;                     // 倒角端面落差 Δ
        const double xd   = dEl * std::sqrt(3.0);              // 倒角弧/棱线起点 x
        const double tip  = k + length;                        // 杆端（总长 l = k + 杆长）
        const double b    = std::min(2.0 * d + 6.0, length);   // 螺纹长度（参考）
        const double lg   = std::max(0.0, length - b);         // 夹紧长度（含不完整螺纹）
        const double u    = std::min(lg, 0.35 * d);            // 不完整螺纹长度（≈2P 近似）
        const double ls   = lg - u;                            // 光杆长度
        const double c      = 0.15 * d;                                  // 末端倒角轴向长度
        const double chamA  = 30.0;                                      // 杆端倒角半角（要求②：15°~30°）
        const double chamRad = c * std::tan(chamA * K_PI / 180.0);       // 杆端倒角径向落差
        const double d1   = 0.85 * d;                          // 螺纹小径（细实线）
        const double rF   = std::max(0.25, 0.05 * d);          // 头下过渡圆角 r
        const double dw   = 0.9 * s;                           // 支承面直径（近似）
        const double xc   = tip + s + R;                       // 端面图中心
        // —— 头部左端三段弧（要求②：头部高度四等分——上/下 1/4 为侧棱面截交弧，
        //      中 1/2 为正棱面截交弧；30° 倒角锥截交双曲线按习惯以圆弧代替）——
        const double yq    = R / 2.0;                                    // 四等分线 y=±e/4
        const double rhoM  = (xd * xd + yq * yq) / (2.0 * xd);           // 中弧半径（圆心 (ρm,0)）
        const double aMdeg = std::atan2(yq, xd - rhoM) * 180.0 / K_PI;   // 中弧端角（90°~180°）
        const double cxS   = (xd * xd + (R / 4.0) * (R / 4.0)) / (2.0 * xd);  // 侧弧圆心 x（=半径）
        const double aSdeg = std::atan2(R / 4.0, xd - cxS) * 180.0 / K_PI;    // 侧弧端角（90°~180°）
        // 真实数值标注（要求③）：一位小数，整数去掉 ".0" 尾
        auto fmt = [](double v) {
            QString t = QString::number(v, 'f', 1);
            if (t.endsWith(QStringLiteral(".0"))) t.chop(2);
            return t;
        };
        // —— 主视：上/下对称链（倒角侧弧→顶/底边→支承面上/下缘→过渡圆角→光杆）——
        for (int sg = 0; sg < 2; ++sg) {
            const double sg1 = (sg == 0) ? 1.0 : -1.0;
            // 倒角侧弧（要求②）：上/下 1/4 区域——过 (xd,±R/2) 与 (xd,±R)、左凸至 (0,±3R/4)
            out.push_back(sg == 0
                ? mkArc(Vec3(cxS,  3.0 * R / 4.0, 0), cxS, aSdeg, 360.0 - aSdeg,
                        QStringLiteral("螺栓头部倒角弧"))
                : mkArc(Vec3(cxS, -3.0 * R / 4.0, 0), cxS, -(360.0 - aSdeg), -aSdeg,
                        QStringLiteral("螺栓头部倒角弧")));
            out.push_back(mkPolyline({ Vec3(xd, sg1 * R, 0), Vec3(k, sg1 * R, 0),
                                       Vec3(k, sg1 * (d / 2.0 + rF), 0) },
                                     QStringLiteral("螺栓头部轮廓"), false));
            out.push_back(sg == 0
                ? mkArc(Vec3(k + rF,  d / 2.0 + rF, 0), rF, 180.0, 270.0, QStringLiteral("头部过渡圆角"))
                : mkArc(Vec3(k + rF, -(d / 2.0 + rF), 0), rF, 90.0, 180.0, QStringLiteral("头部过渡圆角")));
            if (ls > 0.05)
                out.push_back(mkLine(Vec3(k + rF, sg1 * d / 2.0, 0), Vec3(k + ls, sg1 * d / 2.0, 0),
                                     QStringLiteral("螺栓杆部轮廓")));
            out.push_back(mkPolyline({ Vec3(k + ls, sg1 * d / 2.0, 0), Vec3(tip - c, sg1 * d / 2.0, 0),
                                       Vec3(tip, sg1 * (d / 2.0 - chamRad), 0) },
                                     QStringLiteral("螺栓杆部轮廓"), false));
        }
        // 头部倒角中弧（要求②）：中间 1/2 区域——过 (xd,±R/2)、左凸至端面 (0,0)
        out.push_back(mkArc(Vec3(rhoM, 0, 0), rhoM, aMdeg, 360.0 - aMdeg,
                            QStringLiteral("螺栓头部倒角中弧")));
        // 头部端面竖线（要求①：补画缺失竖线，x=0 处与三段倒角弧相切于 (0,0)/(0,±3R/4)）
        out.push_back(mkLine(Vec3(0, -3.0 * R / 4.0, 0), Vec3(0, 3.0 * R / 4.0, 0),
                             QStringLiteral("螺栓头部端面竖线")));
        out.push_back(mkLine(Vec3(tip, -(d / 2.0 - chamRad), 0), Vec3(tip, d / 2.0 - chamRad, 0),
                             QStringLiteral("螺杆端面")));
        if (u > 0.05)   // 不完整螺纹过渡（波浪线，细）
            out.push_back(mkLine(Vec3(k + ls, -d / 2.0, 0), Vec3(k + ls, d / 2.0, 0),
                                 QStringLiteral("螺纹过渡线"), LineStyle::Wave));
        // 头部棱线（侧视中间两棱投影于 ±e/4 半高处）
        out.push_back(mkLine(Vec3(xd,  R / 2.0, 0), Vec3(k,  R / 2.0, 0), QStringLiteral("螺栓头部棱线")));
        out.push_back(mkLine(Vec3(xd, -R / 2.0, 0), Vec3(k, -R / 2.0, 0), QStringLiteral("螺栓头部棱线")));
        // 螺纹小径（完整螺纹段，细实线 ×2）
        for (int sg = 0; sg < 2; ++sg) {
            const double sg1 = (sg == 0) ? 1.0 : -1.0;
            Element mn = mkLine(Vec3(k + lg, sg1 * d1 / 2.0, 0), Vec3(tip - c, sg1 * d1 / 2.0, 0),
                                QStringLiteral("螺纹小径"));
            mkThin(mn);
            out.push_back(std::move(mn));
        }
        // 螺纹根部线（颈部区域少量根线，细实线，与放大图细线风格一致；示意绘制）
        {
            const int nRoot = 4;                       // 示意根线截面数（少量）
            const double xN0 = k + lg, xN1 = tip - c;  // 完整螺纹区（颈部）
            int drawn = 0;
            if (xN1 - xN0 > 1.0) {
                for (int i = 0; i < nRoot; ++i) {
                    const double xr = xN0 + (i + 1) * (xN1 - xN0) / double(nRoot + 1);
                    for (int sg = 0; sg < 2; ++sg) {
                        const double sg1 = (sg == 0) ? 1.0 : -1.0;
                        Element rl = mkLine(Vec3(xr, sg1 * d / 2.0, 0),
                                            Vec3(xr, sg1 * d1 / 2.0, 0),
                                            QStringLiteral("螺纹根部线"));
                        mkThin(rl);
                        out.push_back(std::move(rl));
                        ++drawn;
                    }
                }
            }
            out.push_back(mkMText(Vec3((xN0 + xN1) / 2.0, d / 2.0 + 3.0, 0),
                                  QStringLiteral("螺纹根部线 ×%1（示意）").arg(drawn), 2.5, 0,
                                  QStringLiteral("根部线标注")));
        }
        // 轴线（双点划线，两端延伸 2mm）
        out.push_back(mkLine(Vec3(-2.0, 0, 0), Vec3(tip + 2.0, 0, 0),
                             QStringLiteral("螺栓中心线"), LineStyle::DashDotDot));
        // 局部放大圈 X（细实线）+ 标识
        const double rdet = 0.45 * (k + R);
        Element detC = mkCircle(Vec3(0.4 * k, 0.4 * R, 0), rdet, QStringLiteral("局部放大圈"));
        mkThin(detC);
        out.push_back(std::move(detC));
        out.push_back(mkMText(Vec3(0.4 * k + rdet + 1.2, 0.4 * R + rdet + 1.2, 0),
                              QStringLiteral("X"), 3.5, 4, QStringLiteral("视图标识")));
        // —— 主视标注（要求③：全部真实数值，text 覆盖显示）——
        out.push_back(mkDim(DimensionType::Horizontal,
                            { Vec3(k, -R, 0), Vec3(k + ls, -R, 0), Vec3(k + ls / 2.0, -(R + 5.0), 0) },
                            fmt(ls)));
        out.push_back(mkDim(DimensionType::Horizontal,
                            { Vec3(k, -R, 0), Vec3(k + lg, -R, 0), Vec3(k + lg / 2.0, -(R + 9.0), 0) },
                            fmt(lg)));
        out.push_back(mkDim(DimensionType::Horizontal,
                            { Vec3(k + lg, -R, 0), Vec3(tip, -R, 0),
                              Vec3(k + lg + b / 2.0, -(R + 12.0), 0) },
                            QStringLiteral("(%1)").arg(fmt(b))));
        out.push_back(mkDim(DimensionType::Horizontal,
                            { Vec3(0, -R, 0), Vec3(k, -R, 0), Vec3(k / 2.0, -(R + 12.0), 0) },
                            fmt(k)));
        out.push_back(mkDim(DimensionType::Horizontal,
                            { Vec3(0, -R, 0), Vec3(tip, -R, 0), Vec3(tip / 2.0, -(R + 16.0), 0) },
                            fmt(tip)));
        if (u > 0.05)
            out.push_back(mkDim(DimensionType::Horizontal,
                                { Vec3(k + ls, -d / 2.0, 0), Vec3(k + lg, -d / 2.0, 0),
                                  Vec3(k + (ls + lg) / 2.0, -(d / 2.0 + 2.0), 0) },
                                fmt(u)));
        out.push_back(mkDim(DimensionType::Vertical,
                            { Vec3(k + ls / 2.0, d / 2.0, 0), Vec3(k + ls / 2.0, -d / 2.0, 0),
                              Vec3(k + ls / 2.0, R + 4.0, 0) },
                            fmt(d)));
        out.push_back(mkDim(DimensionType::Vertical,
                            { Vec3(k + lg + 0.35 * b, d / 2.0, 0), Vec3(k + lg + 0.35 * b, -d / 2.0, 0),
                              Vec3(k + lg + 0.35 * b, R + 4.0, 0) },
                            fmt(d)));
        {   // β 头部倒角角（锥与顶边夹角 30°，真实数值）
            Element bt = mkDim(DimensionType::Angle,
                               { Vec3(xd, R, 0), Vec3(xd - 2.5, R, 0),
                                 Vec3(xd - 2.5 * std::sqrt(3.0) / 2.0, R - 1.25, 0) },
                               QStringLiteral("30°"));
            bt.radius = 2.0;   // 角度尺寸弧半径
            out.push_back(std::move(bt));
        }
        out.push_back(mkDim(DimensionType::Leader,
                            { Vec3(tip - c / 2.0, d / 2.0 - chamRad / 2.0, 0), Vec3(tip + 3.0, R + 2.5, 0) },
                            QStringLiteral("%1×%2°").arg(fmt(c)).arg(int(chamA))));
        // —— 端面图：正六边形（顶点 ±Y → 对边水平）+ 螺纹圆 ——
        {
            std::vector<Vec3> hp;
            hp.reserve(6);
            for (int i = 0; i < 6; ++i) {
                const double a = (90.0 + 60.0 * i) * K_PI / 180.0;
                hp.push_back(Vec3(xc + R * std::cos(a), R * std::sin(a), 0.0));
            }
            out.push_back(mkPolyline(hp, QStringLiteral("螺栓端面六角")));
            // 要求④：头部倒角内切圆——半径 s/2，与六边各边相切
            out.push_back(mkCircle(Vec3(xc, 0, 0), s / 2.0, QStringLiteral("头部倒角内切圆")));
            out.push_back(mkCircle(Vec3(xc, 0, 0), d / 2.0, QStringLiteral("螺纹大径圆")));
            Element minArc = mkArc(Vec3(xc, 0, 0), d1 / 2.0, 45.0, 315.0, QStringLiteral("螺纹小径圆弧"));
            mkThin(minArc);
            out.push_back(std::move(minArc));
            out.push_back(mkLine(Vec3(xc - R - 2.0, 0, 0), Vec3(xc + R + 2.0, 0, 0),
                                 QStringLiteral("螺栓端面中心线"), LineStyle::DashDotDot));
            out.push_back(mkLine(Vec3(xc, -R - 2.0, 0), Vec3(xc, R + 2.0, 0),
                                 QStringLiteral("螺栓端面中心线"), LineStyle::DashDotDot));
            out.push_back(mkDim(DimensionType::Horizontal,
                                { Vec3(xc - s / 2.0, 0, 0), Vec3(xc + s / 2.0, 0, 0),
                                  Vec3(xc, -(R + 3.5), 0) },
                                fmt(s)));
            out.push_back(mkDim(DimensionType::Vertical,
                                { Vec3(xc, -R, 0), Vec3(xc, R, 0), Vec3(xc + R + 3.5, 0, 0) },
                                fmt(e)));
        }
        // —— 局部放大图 X（2:1）：头部倒角 + 头下圆角 + 杆部，两侧波浪折断 ——
        {
            const double m2  = 2.0;                       // 放大比例 2:1
            const double xL  = -1.5, xRr = k + 3.0;       // 放大区域（世界系）
            const double hY  = R + 0.8;
            const double c0x = (xL + xRr) / 2.0;
            const double Yd  = -(R + 16.0) - 8.0 - hY * m2;   // 放大图中心 y（主视尺寸行之下）
            auto T = [&](const Vec3& p) { return Vec3((p.x - c0x) * m2 + c0x, Yd + p.y * m2, 0.0); };
            auto dimT = [&](DimensionType dt, std::vector<Vec3> w, const QString& txt) {
                for (Vec3& p : w) p = T(p);
                out.push_back(mkDim(dt, std::move(w), txt));
            };
            for (int sg = 0; sg < 2; ++sg) {
                const double sg1 = (sg == 0) ? 1.0 : -1.0;
                out.push_back(sg == 0
                    ? mkArc(T(Vec3(cxS,  3.0 * R / 4.0, 0)), cxS * m2, aSdeg, 360.0 - aSdeg,
                            QStringLiteral("螺栓头部倒角弧"))
                    : mkArc(T(Vec3(cxS, -3.0 * R / 4.0, 0)), cxS * m2, -(360.0 - aSdeg), -aSdeg,
                            QStringLiteral("螺栓头部倒角弧")));
                out.push_back(mkPolyline({ T(Vec3(xd, sg1 * R, 0)), T(Vec3(k, sg1 * R, 0)),
                                           T(Vec3(k, sg1 * (d / 2.0 + rF), 0)) },
                                         QStringLiteral("螺栓头部轮廓"), false));
                out.push_back(sg == 0
                    ? mkArc(T(Vec3(k + rF,  d / 2.0 + rF, 0)), rF * m2, 180.0, 270.0, QStringLiteral("头部过渡圆角"))
                    : mkArc(T(Vec3(k + rF, -(d / 2.0 + rF), 0)), rF * m2, 90.0, 180.0, QStringLiteral("头部过渡圆角")));
                out.push_back(mkLine(T(Vec3(k + rF, sg1 * d / 2.0, 0)), T(Vec3(k + 2.5, sg1 * d / 2.0, 0)),
                                     QStringLiteral("螺栓杆部轮廓")));
            }
            out.push_back(mkArc(T(Vec3(rhoM, 0, 0)), rhoM * m2, aMdeg, 360.0 - aMdeg,
                                QStringLiteral("螺栓头部倒角中弧")));
            out.push_back(mkLine(T(Vec3(xL, -R - 0.5, 0)), T(Vec3(xL, R + 0.5, 0)),
                                 QStringLiteral("折断线"), LineStyle::Wave));
            out.push_back(mkLine(T(Vec3(k + 2.5, -(d / 2.0 + 0.8), 0)), T(Vec3(k + 2.5, d / 2.0 + 0.8, 0)),
                                 QStringLiteral("折断线"), LineStyle::Wave));
            out.push_back(mkLine(T(Vec3(xL, 0, 0)), T(Vec3(xRr, 0, 0)),
                                 QStringLiteral("放大图轴线"), LineStyle::DashDotDot));
            // 放大图标注（要求③：真实数值/R 值，与附图位置对应）
            dimT(DimensionType::Horizontal,
                 { Vec3(0, R, 0), Vec3(k, R, 0), Vec3(k / 2.0, R + 2.2, 0) }, fmt(k));
            dimT(DimensionType::Horizontal,
                 { Vec3(k + rF, -(d / 2.0 + 1.2), 0), Vec3(k + 2.5, -(d / 2.0 + 1.2), 0),
                   Vec3(k + (rF + 2.5) / 2.0, -(d / 2.0 + 2.8), 0) }, fmt(2.5 - rF));
            dimT(DimensionType::Leader,
                 { Vec3(k + rF * (1.0 - 0.7071), d / 2.0 + rF * 0.2929, 0),
                   Vec3(k + 1.8, d / 2.0 + 2.4, 0) }, QStringLiteral("R%1").arg(fmt(rF)));
            dimT(DimensionType::Vertical,
                 { Vec3(k, -R, 0), Vec3(k, R, 0), Vec3(k + 3.4, 0, 0) }, fmt(e));
            dimT(DimensionType::Vertical,
                 { Vec3(k, -dw / 2.0, 0), Vec3(k, dw / 2.0, 0), Vec3(k + 2.2, 0, 0) }, fmt(dw));
            dimT(DimensionType::Leader,   // 倒角中弧半径（引线指向左凸弧顶附近）
                 { Vec3(rhoM * (1.0 + std::cos(160.0 * K_PI / 180.0)),
                        rhoM * std::sin(160.0 * K_PI / 180.0), 0),
                   Vec3(-1.2, 2.6, 0) },
                 QStringLiteral("R%1").arg(fmt(rhoM)));
            out.push_back(mkMText(Vec3(c0x, Yd + hY * m2 + 2.5, 0),
                                  QStringLiteral("X"), 3.5, 4, QStringLiteral("视图标识")));
            // 技术要求（要求①）：置于放大图最下方标注/引线之下空白区，不与图形重叠
            out.push_back(mkMText(Vec3(0.0, Yd - 2.0 * R - 10.0, 0),
                                  QStringLiteral("技术要求：\n"
                                                "1. 螺纹公差 6g（GB/T 197）；\n"
                                                "2. 产品等级 A（GB/T 3103.1）；\n"
                                                "3. β=30°，u≤2P，c=0.15d，r 为头下过渡圆角；\n"
                                                "4. 末端形状由制造者选择（允许倒角或辗制）；\n"
                                                "5. 其余验收要求按 GB/T 5782。"),
                                  3.5, 0, QStringLiteral("技术要求")));
        }
        return true;
    }
};

// 六角螺母（正视）：正六边形(对边 s) + 螺纹孔圆(≈d)。
class NutGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("nut"); }
    QString displayName() const override { return QStringLiteral("六角螺母"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::HexNut, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("六角螺母：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        out.push_back(mkPolyline(hexPts(Vec3(0, 0, 0), sp.acrossFlats), QStringLiteral("螺母六角")));
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.innerD / 2.0, QStringLiteral("螺母螺纹孔")));
        return true;
    }
};

// 平垫圈（正视）：外圆(d2) + 内圆(d1)。
class WasherGenerator : public PartGenerator {
public:
    QString id() const override { return QStringLiteral("washer"); }
    QString displayName() const override { return QStringLiteral("平垫圈"); }
    bool generate(const QVariantMap& params, std::vector<Element>& out, QString* note) const override {
        const double nominal = params.value(QStringLiteral("nominal"), 6.0).toDouble();
        const FastenerSpec sp = GbFastenerTable::lookup(FastenerKind::PlainWasher, nominal);
        if (!sp.valid && note)
            *note = QStringLiteral("平垫圈：公称 M%1 无对应 GB 规格，回落最近规格 M%2")
                        .arg(nominal).arg(sp.nominal);
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.outerD / 2.0, QStringLiteral("垫圈外圆")));
        out.push_back(mkCircle(Vec3(0, 0, 0), sp.innerD / 2.0, QStringLiteral("垫圈内圆")));
        return true;
    }
};

} // namespace

// ------------------------------------------------------------
// PartLibrary 单例
// ------------------------------------------------------------
PartLibrary& PartLibrary::instance() {
    static PartLibrary s;
    return s;
}

PartLibrary::PartLibrary() {
    // 构造时注册内建零件（P5：GB 紧固件出库）
    registerGenerator(std::make_shared<GearGenerator>());
    registerGenerator(std::make_shared<BoltGenerator>());
    registerGenerator(std::make_shared<NutGenerator>());
    registerGenerator(std::make_shared<WasherGenerator>());
}

void PartLibrary::registerGenerator(std::shared_ptr<PartGenerator> g) {
    if (g) gens_[g->id()] = std::move(g);
}

const PartGenerator* PartLibrary::find(const QString& id) const {
    auto it = gens_.find(id);
    return (it != gens_.end()) ? it.value().get() : nullptr;
}

bool PartLibrary::generate(const QString& id, const QVariantMap& params, std::vector<Element>& out, QString* note) const {
    const PartGenerator* g = find(id);
    if (!g) return false;
    return g->generate(params, out, note);
}

bool PartLibrary::checkout(const QString& id, const QVariantMap& params, BlockDef& defOut, QString* note) const {
    const PartGenerator* g = find(id);
    if (!g) return false;
    std::vector<Element> geom;
    if (!g->generate(params, geom, note)) return false;
    // 实例化块定义（引用式）：几何已按 WCS 生成、基点取原点
    defOut = block::makeDefFromEntities(id, Vec3(0, 0, 0), geom, g->displayName());
    // 追加属性定义（标准 / 公称 / 名称），供 BOM 提取（block::extractAttributes）
    const double nominal = params.value(QStringLiteral("nominal"), 0.0).toDouble();
    auto addAttr = [&](const QString& tag, const QString& val) {
        AttributeDef a;
        a.tag = tag; a.prompt = tag; a.defaultValue = val;
        a.pos = Vec3(0, 0, 0); a.height = 3.5; a.constant = true; a.invisible = true;
        defOut.attdefs.push_back(a);
    };
    if (nominal > 0.0) {
        const FastenerKind kind = GbFastenerTable::kindFromName(id);
        addAttr(QStringLiteral("STANDARD"), GbFastenerTable::standardTag(kind, nominal));
    }
    addAttr(QStringLiteral("NOMINAL"), QString::number(nominal, 'f', 1));
    addAttr(QStringLiteral("NAME"), g->displayName());
    return true;
}

QVector<QString> PartLibrary::ids() const {
    return gens_.keys().toVector();
}

} // namespace SketchCAD
