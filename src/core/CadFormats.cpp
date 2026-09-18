#include "CadFormats.h"
#include "Geometry.h"
#include "DimStyle.h"
#include <QFileInfo>
#include <QStringList>
#include <QDateTime>
#include <QSet>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace SketchCAD {
namespace cadfmt {

// ----------------------------- DXF 编码（GBK） -----------------------------
// DXF R12 没有 UTF-8 的概念：文件字符编码由 $DWGCODEPAGE 声明，中文环境是 ANSI_936（GBK）。
// 这里直接用 Win32 代码页转换，不引入任何编码库（Qt6 已把 QTextCodec 移出 QtCore）。
// 非 Windows 平台回落 UTF-8（此时文件头声明 ANSI_1252，见 toDxf）。
static QByteArray toGbk(const QString& s) {
#ifdef Q_OS_WIN
    const wchar_t* w = reinterpret_cast<const wchar_t*>(s.utf16());
    const int need = ::WideCharToMultiByte(936, 0, w, s.size(), nullptr, 0, nullptr, nullptr);
    if (need > 0) {
        QByteArray out(need, 0);
        ::WideCharToMultiByte(936, 0, w, s.size(), out.data(), need, nullptr, nullptr);
        return out;
    }
#endif
    return s.toUtf8();
}

static QString fromGbk(const QByteArray& b) {
#ifdef Q_OS_WIN
    const int need = ::MultiByteToWideChar(936, 0, b.constData(), b.size(), nullptr, 0);
    if (need > 0) {
        QString s(need, QChar(0));
        ::MultiByteToWideChar(936, 0, b.constData(), b.size(),
                              reinterpret_cast<wchar_t*>(s.data()), need);
        return s;
    }
#endif
    return QString::fromLocal8Bit(b);
}

// 读 DXF 字节时的编码判定：优先看文件头声明的 $DWGCODEPAGE（ANSI_936/950/949 = 中日韩双字节）；
// 没声明就先按 UTF-8 解，出现替换字符（U+FFFD）说明不是 UTF-8，再回落 GBK。
static QString decodeDxfBytes(const QByteArray& data) {
    const QByteArray head = data.left(8192).toUpper();
    if (head.contains("ANSI_936") || head.contains("ANSI_950") || head.contains("ANSI_949"))
        return fromGbk(data);
    const QString u = QString::fromUtf8(data);
    if (!u.contains(QChar(0xFFFD))) return u;
    return fromGbk(data);
}

// ----------------------------- 格式判定 -----------------------------
CadFormat formatFromSuffix(const QString& suffix) {
    const QString s = suffix.trimmed().toLower();
    if (s == "json" || s == "sketch") return CadFormat::Json;
    if (s == "dxf")  return CadFormat::Dxf;
    if (s == "step" || s == "stp") return CadFormat::Step;
    if (s == "dwg")  return CadFormat::Dwg;
    return CadFormat::Unknown;
}

CadFormat formatFromPath(const QString& path) {
    return formatFromSuffix(QFileInfo(path).suffix());
}

QString defaultSuffix(CadFormat f) {
    switch (f) {
        case CadFormat::Json: return QStringLiteral("json");
        case CadFormat::Dxf:  return QStringLiteral("dxf");
        case CadFormat::Step: return QStringLiteral("step");
        case CadFormat::Dwg:  return QStringLiteral("dwg");
        case CadFormat::Unknown: break;
    }
    return QStringLiteral("json");
}

QString formatName(CadFormat f) {
    switch (f) {
        case CadFormat::Json: return QStringLiteral("草图 JSON");
        case CadFormat::Dxf:  return QStringLiteral("DXF");
        case CadFormat::Step: return QStringLiteral("STEP");
        case CadFormat::Dwg:  return QStringLiteral("DWG");
        case CadFormat::Unknown: break;
    }
    return QStringLiteral("未知格式");
}

bool canImport(CadFormat f) { return f == CadFormat::Json || f == CadFormat::Dxf; }

// =====================================================================
//  DXF R12 ASCII 写出
// =====================================================================

// 把 RGB 近似到 AutoCAD 标准色号 ACI 1..9。
// 真彩色写在组码 420 里，但老版本 / 部分轻量看图软件只认 62，所以两个都写，
// 62 作为回退不会让图元变成"不可见的默认色"。
static int aciFromRgb(const QColor& c) {
    struct Ref { int aci, r, g, b; };
    static const Ref table[] = {
        {1, 255, 0, 0},   {2, 255, 255, 0}, {3, 0, 255, 0},
        {4, 0, 255, 255}, {5, 0, 0, 255},   {6, 255, 0, 255},
        {7, 0, 0, 0},     {7, 255, 255, 255},
        {8, 128, 128, 128}, {9, 192, 192, 192},
    };
    int best = 7;
    long long bestD = -1;
    for (const Ref& t : table) {
        long long dr = c.red() - t.r, dg = c.green() - t.g, db = c.blue() - t.b;
        long long d = dr * dr + dg * dg + db * db;
        if (bestD < 0 || d < bestD) { bestD = d; best = t.aci; }
    }
    return best;
}

// ACI 1..9 -> RGB（DXF 回读用，与上面 aciFromRgb 的表互为反函数）
static QColor rgbFromAci(int aci) {
    switch (aci) {
        case 1: return QColor(255, 0, 0);
        case 2: return QColor(255, 255, 0);
        case 3: return QColor(0, 255, 0);
        case 4: return QColor(0, 255, 255);
        case 5: return QColor(0, 0, 255);
        case 6: return QColor(255, 0, 255);
        case 7: return QColor(26, 29, 40);
        case 8: return QColor(128, 128, 128);
        case 9: return QColor(192, 192, 192);
        default: return QColor(26, 29, 40);
    }
}

// 图层名净化：DXF 是「组码行 + 值行」的文本格式，任何组码的值里都不能含换行，
// 否则后续所有组码都会错位（整份文件作废）。图层名同理。
static QString dxfSafe(const QString& s) {
    QString t = s;
    t.replace("\r\n", " "); t.replace('\r', ' '); t.replace('\n', ' ');
    t.replace('\t', ' ');
    return t;
}
static QString dxfLayer(const Element& e) {
    const QString ly = dxfSafe(e.layer).trimmed();
    return ly.isEmpty() ? QStringLiteral("0") : ly;
}

QByteArray toDxf(const std::vector<Element>& elems, QString* err, double drawingScale) {
    if (err) err->clear();

    // 标注数字要还原成实物尺寸：把绘图比例交给 dim::build
    dim::Style dimSt;
    dimSt.drawingScale = (drawingScale > 1e-12) ? drawingScale : 1.0;

    QStringList L;
    const auto num = [](double v) { return QString::number(v, 'f', 6); };

    // ---- 图层清单 ----
    QSet<QString> layers;
    layers.insert(QStringLiteral("0"));
    for (const auto& e : elems) layers.insert(dxfLayer(e));

    // ---- 图形范围（供看图软件 zoom-extents）----
    QRectF ext(0.0, 0.0, 0.0, 0.0);
    bool hasExt = false;
    for (const auto& e : elems) {
        const QRectF b = geo::elementBounds(e);
        ext = hasExt ? ext.united(b) : b;
        hasExt = true;
    }
    const QString ymin = num(ext.top()), ymax = num(ext.bottom());

    // ---- HEADER ----
    // 显式声明版本是必须的：本文件用的是 R12 实体与组码，不声明版本解析器只能猜，
    // 猜错就会出现「实体与版本不自洽」的解析失败（例如 LWPOLYLINE 在 R12 里不存在）。
    L << "0" << "SECTION" << "2" << "HEADER"
      << "9" << "$ACADVER" << "1" << "AC1009"
      << "9" << "$DWGCODEPAGE" << "3" << "@@CP@@"
      << "9" << "$INSBASE" << "10" << "0.0" << "20" << "0.0" << "30" << "0.0"
      << "9" << "$EXTMIN"  << "10" << num(ext.left())  << "20" << ymin << "30" << "0.0"
      << "9" << "$EXTMAX"  << "10" << num(ext.right()) << "20" << ymax << "30" << "0.0"
      << "9" << "$LIMMIN"  << "10" << "0.0" << "20" << "0.0"
      << "9" << "$LIMMAX"  << "10" << num(ext.right()) << "20" << ymax
      << "0" << "ENDSEC";

    // ---- TABLES：线型表（CONTINUOUS 是图层必须引用的线型）+ 图层表 ----
    // 注意：R12 没有真彩色(420) 与线宽(370) 组码，所以这里只登记 ACI 颜色，
    // 颜色信息经 62 组码近似到最近的 ACI 色号（本程序调色板本就是标准色，损失可忽略）。
    L << "0" << "SECTION" << "2" << "TABLES"
      << "0" << "TABLE" << "2" << "LTYPE" << "70" << "1"
      << "0" << "LTYPE" << "2" << "CONTINUOUS" << "70" << "64"
      << "3" << "Solid line" << "72" << "65" << "73" << "0" << "40" << "0.0"
      << "0" << "ENDTAB"
      << "0" << "TABLE" << "2" << "LAYER" << "70" << QString::number(layers.size());
    for (const QString& ly : layers)
        L << "0" << "LAYER" << "2" << ly
          << "70" << "0" << "62" << "7" << "6" << "CONTINUOUS";
    L << "0" << "ENDTAB" << "0" << "ENDSEC";

    L << "0" << "SECTION" << "2" << "ENTITIES";

    // 实体公共头：图层 + ACI 颜色
    const auto ent = [&](const QString& type, const Element& e) {
        L << "0" << type << "8" << dxfLayer(e) << "62" << QString::number(aciFromRgb(e.color));
    };

    // 多段线：R12 用 POLYLINE/VERTEX/SEQEND（LWPOLYLINE 是 R13+ 实体，不能出现在 R12 文件里）。
    // arcFlags[i]=true 表示 pts[i]→pts[i+1] 为圆弧段，用 VERTEX 的 42 组码（bulge=tan(含角/4)）表达，
    // 保留真实弧段而不是离散成折线。
    const auto polylineEx = [&](const Element& e, const std::vector<Vec3>& pts,
                               const std::vector<bool>& arcFlags, bool closed) {
        if (pts.size() < 2) return;
        ent(QStringLiteral("POLYLINE"), e);
        L << "66" << "1" << "70" << (closed ? "1" : "0");
        const int segCount = closed ? (int)pts.size() : (int)pts.size() - 1;
        for (int i = 0; i < (int)pts.size(); ++i) {
            L << "0" << "VERTEX" << "8" << dxfLayer(e)
              << "10" << num(pts[i].x) << "20" << num(pts[i].y);
            if (i < (int)arcFlags.size() && arcFlags[i] && i < segCount) {
                const geo::ArcSeg as = geo::arcThrough(pts, i, closed);
                if (as.valid && as.radius > 1e-12) {
                    const double bulge = std::tan(as.sweepDeg * M_PI / 180.0 / 4.0);
                    if (std::fabs(bulge) > 1e-12) L << "42" << QString::number(bulge, 'f', 9);
                }
            }
        }
        L << "0" << "SEQEND" << "8" << dxfLayer(e);
    };
    const auto polyline = [&](const Element& e, const std::vector<Vec3>& pts, bool closed) {
        polylineEx(e, pts, std::vector<bool>(), closed);
    };

    // 文本：把多行内容按行拆成多个 TEXT 实体。
    // 绝不能把含 \n 的字符串整段写进组码 1 —— 那会把值行劈成两行，后续组码全部错位，
    // 整份 DXF 直接不可解析（标注数字就是 "xx m\n(xx ft)" 这种双行文本）。
    const auto textLines = [&](const Element& e, const QString& raw, const Vec3& pos,
                               double h, double rot, bool centered) {
        QString t = raw;
        t.replace("\r\n", "\n");
        t.replace('\r', '\n');
        const QStringList parts = t.split('\n');
        const double lineH = (h > 0.1 ? h : 3.5) * 1.3;
        for (int i = 0; i < parts.size(); ++i) {
            const QString line = dxfSafe(parts.at(i));
            if (line.trimmed().isEmpty()) continue;
            const double y = pos.y - i * lineH;
            ent(QStringLiteral("TEXT"), e);
            L << "10" << num(pos.x) << "20" << num(y) << "40" << num(h);
            if (std::fabs(rot) > 1e-9) L << "50" << num(rot);
            if (centered) L << "72" << "1" << "73" << "2" << "11" << num(pos.x) << "21" << num(y);
            L << "1" << line;
        }
    };

    int splineSampled = 0;
    for (const auto& e : elems) {
        if (e.type == ElementType::Line && e.pts.size() >= 2) {
            ent(QStringLiteral("LINE"), e);
            L << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "11" << num(e.pts[1].x) << "21" << num(e.pts[1].y) << "31" << "0.0";
        } else if (e.type == ElementType::Circle && !e.pts.empty()) {
            ent(QStringLiteral("CIRCLE"), e);
            L << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "40" << num(e.radius);
        } else if (e.type == ElementType::Arc && !e.pts.empty()) {
            ent(QStringLiteral("ARC"), e);
            L << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "40" << num(e.radius)
              << "50" << num(e.startAngle) << "51" << num(e.endAngle);
        } else if (e.type == ElementType::Ellipse && !e.pts.empty()) {
            // R12 无原生 ELLIPSE 实体，按椭圆母线采样成闭合多段线
            polyline(e, geo::sampleEllipse(e.pts[0], e.radius, e.radiusY, e.rotation, 96), true);
        } else if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
            const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
            polyline(e, {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)},
                     e.polyClosed());
        } else if (e.type == ElementType::Polygon && e.pts.size() >= 2) {
            polyline(e, e.pts, e.polyClosed());   // 剪切过的轮廓导成开放多段线
        } else if (e.type == ElementType::Polyline && e.pts.size() >= 2) {
            polylineEx(e, e.pts, e.arcFlags, e.closed);
        } else if (e.type == ElementType::Dimension) {
            // 标注按 GB 生成的几何「炸开」导出：尺寸线/界线用 LINE、箭头用实心 SOLID、
            // 数字用 TEXT。这样任何看图软件不依赖标注样式表也能显示成国标的样子。
            const dim::Geom g = dim::build(e, dimSt);
            if (!g.valid) continue;
            const auto seg = [&](const Vec3& a, const Vec3& b) {
                ent(QStringLiteral("LINE"), e);
                L << "10" << num(a.x) << "20" << num(a.y) << "30" << "0.0"
                  << "11" << num(b.x) << "21" << num(b.y) << "31" << "0.0";
            };
            for (const auto& s : g.ext)  seg(s.first, s.second);
            for (const auto& s : g.line) seg(s.first, s.second);
            for (size_t i = 1; i < g.arcPoly.size(); ++i) seg(g.arcPoly[i - 1], g.arcPoly[i]);
            const dim::Style st;
            for (const auto& ar : g.arrows) {
                const double rad = ar.dirDeg * M_PI / 180.0;
                const double cx = std::cos(rad), sy = std::sin(rad);
                const Vec3 back(ar.tip.x - cx * st.arrowLen, ar.tip.y - sy * st.arrowLen, 0);
                const Vec3 off(-sy * st.arrowWidth * 0.5, cx * st.arrowWidth * 0.5, 0);
                const Vec3 p1 = back + off, p2 = back - off;
                // SOLID 的第 4 点与第 3 点重合即退化为三角形（DXF 的标准写法）
                ent(QStringLiteral("SOLID"), e);
                L << "10" << num(ar.tip.x) << "20" << num(ar.tip.y)
                  << "11" << num(p1.x) << "21" << num(p1.y)
                  << "12" << num(p2.x) << "22" << num(p2.y)
                  << "13" << num(p2.x) << "23" << num(p2.y);
            }
            if (!g.label.isEmpty())
                textLines(e, g.label, g.textPos, st.textHeight, g.textRotDeg, true);
        } else if (e.type == ElementType::Spline && e.pts.size() >= 2) {
            // 样条按屏幕上看到的那条 Catmull-Rom 曲线离散后导出。
            // 直接导原始控制点会让 DXF 里变成折线 —— 跟屏幕显示不一致，
            // 下游看到的形状就不是设计者画的那条曲线了。
            polyline(e, geo::sampleSpline(e.pts, 16), false);
            ++splineSampled;
        } else if (e.type == ElementType::Text && !e.pts.empty() && !e.text.isEmpty()) {
            // 标题栏栏目等文本：字高用 radius 字段的 mm 值
            textLines(e, e.text, e.pts[0], e.radius > 0.1 ? e.radius : 3.5, 0.0, false);
        }
    }
    L << "0" << "ENDSEC" << "0" << "EOF";
    (void)splineSampled;

    // ---- 定编码：先按占位符生成，再依内容是否有非 ASCII 决定 $DWGCODEPAGE 并编码 ----
    // （占位符与代码页名都是 ASCII，替换不会改变"是否含非 ASCII"的判定，故可两步走）
    QString body = L.join("\r\n");
    bool asciiOnly = true;
    for (const QChar& c : body)
        if (c.unicode() > 0x7F) { asciiOnly = false; break; }
    if (asciiOnly) {
        body.replace(QStringLiteral("@@CP@@"), QStringLiteral("ANSI_1252"));
        return body.toLatin1();
    }
    body.replace(QStringLiteral("@@CP@@"), QStringLiteral("ANSI_936"));
    return toGbk(body);
}

// DWG 兼容写出：内容与 DXF 完全一致。
// 原生二进制 DWG 需 ODA Drawings SDK（商业）或 LibreDWG（GPL），本项目均未链接，
// 因此这里不做"伪造二进制头"的事——直接给出 DXF 内容，由调用方决定是否以 .dwg 落盘
// 并务必向用户说明该限制（AutoCAD 可通过 DXF 打开后「另存为」得到真正的 DWG）。
QByteArray toDwg(const std::vector<Element>& elems, QString* err, double drawingScale) {
    // err 只承载"失败原因"；DWG 的限制属于正常情况，由 UI 在保存前弹窗说明，
    // 不在这里混入 err，避免调用方把"成功"误判成"失败"。
    QByteArray out = toDxf(elems, err, drawingScale);
    return out;
}

// =====================================================================
//  STEP (ISO 10303-21, AP214) 写出
// =====================================================================

namespace {
// STEP 实数记法：保证带小数点（STEP 的 REAL 必须有小数点）
inline QString sreal(double v) {
    QString s = QString::number(v, 'f', 6);
    if (!s.contains('.')) s += ".";
    return s;
}
// STEP 字符串转义：单引号翻倍
inline QString sstr(const QString& s) { QString t = s; t.replace('\'', "''"); return t; }
}

QString toStep(const std::vector<Element>& elems, StepStats* stats, QString* err, double drawingScale) {
    if (err) err->clear();
    if (stats) { stats->curves = 0; stats->skippedText = 0; }

    // 同 toDxf：标注数字须还原为实物尺寸
    dim::Style dimSt;
    dimSt.drawingScale = (drawingScale > 1e-12) ? drawingScale : 1.0;

    const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODate);

    int n = 0;
    const auto N = [&n] { return ++n; };
    const auto ref = [](int id) { return QStringLiteral("#%1").arg(id); };

    QStringList pre, geom, curveRefs;

    // ---- 产品结构（AP214 automotive_design 骨架）----
    const int cAppCtx = N();
    pre << ref(cAppCtx) + "=APPLICATION_CONTEXT('automotive design');";
    pre << ref(N()) + QString("=APPLICATION_PROTOCOL_DEFINITION('international standard','automotive_design',2010,%1);").arg(ref(cAppCtx));
    const int cProdCtx = N();
    pre << ref(cProdCtx) + QString("=PRODUCT_CONTEXT('',%1,'mechanical');").arg(ref(cAppCtx));
    const int cProd = N();
    pre << ref(cProd) + QString("=PRODUCT('SKETCH','SKETCH','',(%1));").arg(ref(cProdCtx));
    const int cPdf = N();
    pre << ref(cPdf) + QString("=PRODUCT_DEFINITION_FORMATION('','',%1);").arg(ref(cProd));
    const int cPdCtx = N();
    pre << ref(cPdCtx) + QString("=PRODUCT_DEFINITION_CONTEXT('part definition',%1,'design');").arg(ref(cAppCtx));
    const int cPd = N();
    pre << ref(cPd) + QString("=PRODUCT_DEFINITION('design','',%1,%2);").arg(ref(cPdf)).arg(ref(cPdCtx));
    pre << ref(N()) + QString("=PRODUCT_DEFINITION_SHAPE('','',%1);").arg(ref(cPd));

    // ---- 单位与几何上下文（2D：图元坐标即图纸毫米）----
    const int cLenUnit = N();
    pre << ref(cLenUnit) + "=(LENGTH_UNIT()NAMED_UNIT(*)SI_UNIT(.MILLI.,.METRE.));";
    const int cAngUnit = N();
    pre << ref(cAngUnit) + "=(NAMED_UNIT(*)PLANE_ANGLE_UNIT()SI_UNIT($,.RADIAN.));";
    const int cSolUnit = N();
    pre << ref(cSolUnit) + "=(NAMED_UNIT(*)SOLID_ANGLE_UNIT()SI_UNIT($,.STERADIAN.));";
    const int cUncert = N();
    pre << ref(cUncert) + QString("=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-06),%1,"
                                  "'distance_accuracy_value','confusion accuracy');").arg(ref(cLenUnit));
    const int cCtx = N();
    pre << ref(cCtx) + QString("=(GEOMETRIC_REPRESENTATION_CONTEXT(2)"
                               "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((%1))"
                               "GLOBAL_UNIT_ASSIGNED_CONTEXT((%2,%3,%4))"
                               "REPRESENTATION_CONTEXT('',''));")
                           .arg(ref(cUncert)).arg(ref(cLenUnit)).arg(ref(cAngUnit)).arg(ref(cSolUnit));

    // ---- 几何原语 ----
    const auto addPoint = [&](const Vec3& p) {
        const int i = N();
        geom << ref(i) + QString("=CARTESIAN_POINT('',(%1,%2));").arg(sreal(p.x)).arg(sreal(p.y));
        return i;
    };
    const auto addDir2 = [&](double dx, double dy) {
        const double L = std::hypot(dx, dy);
        if (L < 1e-15) { dx = 1.0; dy = 0.0; }
        else { dx /= L; dy /= L; }
        const int i = N();
        geom << ref(i) + QString("=DIRECTION('',(%1,%2));").arg(sreal(dx)).arg(sreal(dy));
        return i;
    };
    const auto addAxis2 = [&](const Vec3& loc, double rotDeg) {
        const int lp = addPoint(loc);
        const double r = rotDeg * M_PI / 180.0;
        const int dr = addDir2(std::cos(r), std::sin(r));
        const int i = N();
        geom << ref(i) + QString("=AXIS2_PLACEMENT_2D('',%1,%2);").arg(ref(lp)).arg(ref(dr));
        return i;
    };
    const auto addCircle = [&](const Vec3& c, double radius) {
        const int ax = addAxis2(c, 0.0);
        const int i = N();
        geom << ref(i) + QString("=CIRCLE('',%1,%2);").arg(ref(ax)).arg(sreal(radius));
        return i;
    };
    const auto addTrimLine = [&](const Vec3& a, const Vec3& b) {
        const double dx = b.x - a.x, dy = b.y - a.y;
        const double L = std::hypot(dx, dy);
        if (L < 1e-12) return;                        // 退化线段不写
        const int pa = addPoint(a);
        const int dv = addDir2(dx, dy);
        const int vc = N();
        geom << ref(vc) + QString("=VECTOR('',%1,1.);").arg(ref(dv));
        const int ln = N();
        geom << ref(ln) + QString("=LINE('',%1,%2);").arg(ref(pa)).arg(ref(vc));
        const int t1 = N(); geom << ref(t1) + "=PARAMETER_VALUE(0.);";
        const int t2 = N(); geom << ref(t2) + QString("=PARAMETER_VALUE(%1);").arg(sreal(L));
        const int tc = N();
        geom << ref(tc) + QString("=TRIMMED_CURVE('',%1,(%2),(%3),.T.,.PARAMETER.);")
                             .arg(ref(ln)).arg(ref(t1)).arg(ref(t2));
        curveRefs << ref(tc);
        if (stats) ++stats->curves;
    };
    const auto addTrimArc = [&](const Vec3& c, double radius, double a0Deg, double a1Deg) {
        if (radius <= 0.0) return;
        double a0 = a0Deg, a1 = a1Deg;
        while (a1 <= a0 + 1e-9) a1 += 360.0;          // 参数必须沿曲线方向递增（逆时针）
        const int ci = addCircle(c, radius);
        const int t1 = N(); geom << ref(t1) + QString("=PARAMETER_VALUE(%1);").arg(sreal(a0 * M_PI / 180.0));
        const int t2 = N(); geom << ref(t2) + QString("=PARAMETER_VALUE(%1);").arg(sreal(a1 * M_PI / 180.0));
        const int tc = N();
        geom << ref(tc) + QString("=TRIMMED_CURVE('',%1,(%2),(%3),.T.,.PARAMETER.);")
                             .arg(ref(ci)).arg(ref(t1)).arg(ref(t2));
        curveRefs << ref(tc);
        if (stats) ++stats->curves;
    };
    const auto addCurveRef = [&](int id) { curveRefs << ref(id); if (stats) ++stats->curves; };
    const auto addPoly = [&](const std::vector<Vec3>& src, bool closed) {
        std::vector<Vec3> v = src;
        if (closed && v.size() >= 3 && v.front().distTo(v.back()) > 1e-9) v.push_back(v.front());
        if (v.size() < 2) return;
        QStringList ps;
        for (const auto& p : v) ps << ref(addPoint(p));
        const int i = N();
        geom << ref(i) + QString("=POLYLINE('',(%1));").arg(ps.join(","));
        curveRefs << ref(i);
        if (stats) ++stats->curves;
    };
    const auto addEllipse = [&](const Vec3& c, double a, double b, double rotDeg) {
        if (a <= 0.0) return;
        const double bb = (b > 0.0) ? b : a;
        const int ax = addAxis2(c, rotDeg);
        const int i = N();
        geom << ref(i) + QString("=ELLIPSE('',%1,%2,%3);").arg(ref(ax)).arg(sreal(a)).arg(sreal(bb));
        curveRefs << ref(i);
        if (stats) ++stats->curves;
    };

    for (const auto& e : elems) {
        switch (e.type) {
            case ElementType::Line:
                if (e.pts.size() >= 2) addTrimLine(e.pts[0], e.pts[1]);
                break;
            case ElementType::Circle:
                if (!e.pts.empty() && e.radius > 0.0) addCurveRef(addCircle(e.pts[0], e.radius));
                break;
            case ElementType::Arc:
                if (!e.pts.empty()) addTrimArc(e.pts[0], e.radius, e.startAngle, e.endAngle);
                break;
            case ElementType::Ellipse:
                if (!e.pts.empty()) addEllipse(e.pts[0], e.radius, e.radiusY, e.rotation);
                break;
            case ElementType::Rectangle:
                if (e.pts.size() >= 2) {
                    const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
                    addPoly({Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)},
                            e.polyClosed());
                }
                break;
            case ElementType::Polygon:
                if (!e.pts.empty()) addPoly(e.pts, e.polyClosed());
                break;
            case ElementType::Polyline:
                if (!e.pts.empty())
                    addPoly(geo::samplePolyline(e.pts, e.arcFlags, e.closed, 16), false);
                break;
            case ElementType::Spline:
                if (e.pts.size() >= 2) addPoly(geo::sampleSpline(e.pts, 16), false);
                break;
            case ElementType::Dimension: {
                const dim::Geom g = dim::build(e, dimSt);
                if (!g.valid) break;
                for (const auto& s : g.ext)  addTrimLine(s.first, s.second);
                for (const auto& s : g.line) addTrimLine(s.first, s.second);
                for (size_t i = 1; i < g.arcPoly.size(); ++i)
                    addTrimLine(g.arcPoly[i - 1], g.arcPoly[i]);
                const dim::Style st;
                for (const auto& ar : g.arrows) {           // 箭头三角按折线写出（STEP 无 SOLID 实体）
                    const double rad = ar.dirDeg * M_PI / 180.0;
                    const double cx = std::cos(rad), sy = std::sin(rad);
                    const Vec3 back(ar.tip.x - cx * st.arrowLen, ar.tip.y - sy * st.arrowLen, 0);
                    const Vec3 off(-sy * st.arrowWidth * 0.5, cx * st.arrowWidth * 0.5, 0);
                    addPoly({ar.tip, back + off, back - off}, true);
                }
                break;
            }
            case ElementType::Text:
                if (stats) ++stats->skippedText;           // STEP 曲线集不含文字注记
                break;
            case ElementType::Constraint:
            default:
                break;
        }
    }

    // 曲线集合 + 形状表示。
    // 注意 ISO 10303-21 的聚合列表**不允许尾随逗号**：(#1,#2,) 会让严格的第三方解析器报
    // "Unexpected parameter ')'"——而自研的往返测试对此毫无察觉（自己的读入器不校验语法）。
    // 该缺陷由第三方解析器 steputils 独立暴露（见 tools/validate_step.py）。
    const int cCurveSet = N();
    geom << ref(cCurveSet) + QString("=GEOMETRIC_CURVE_SET('SketchCAD',(%1),%2);")
                              .arg(curveRefs.join(","))
                              .arg(ref(cCtx));
    const int cShape = N();
    geom << ref(cShape) + QString("=SHAPE_REPRESENTATION('',(%1),%2);")
                            .arg(ref(cCurveSet)).arg(ref(cCtx));

    QStringList out;
    out << "ISO-10303-21;"
        << "HEADER;"
        << "FILE_DESCRIPTION(('SketchCAD 2D sketch (AP214 curve set)'),'2;1');"
        << QString("FILE_NAME('%1','%2',('SketchCAD'),('SketchCAD'),'SketchCAD 1.0','SketchCAD','');")
               .arg(sstr(QStringLiteral("sketch"))).arg(stamp)
        << "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));"
        << "ENDSEC;"
        << "DATA;";
    out += pre;
    out += geom;
    out << "ENDSEC;" << "END-ISO-10303-21;";
    return out.join("\r\n") + "\r\n";
}

// =====================================================================
//  DXF 回读
// =====================================================================

namespace {

struct DxfPair { int code; QString value; };

std::vector<DxfPair> tokenizeDxf(const QString& text) {
    QString t = text;
    t.replace("\r\n", "\n");
    t.replace('\r', '\n');
    const QStringList lines = t.split('\n');
    std::vector<DxfPair> out;
    size_t i = 0;
    while (i + 1 < (size_t)lines.size()) {
        bool ok = false;
        const int code = lines[(int)i].trimmed().toInt(&ok);
        if (!ok) { ++i; continue; }             // 容错：跳过无法解析为组码的行
        out.push_back({code, lines[(int)i + 1]});
        i += 2;
    }
    return out;
}

// 在实体属性表里取第一个指定组码的数值
double firstNum(const std::vector<DxfPair>& p, int code, double dflt = 0.0) {
    for (const auto& q : p) if (q.code == code) return q.value.trimmed().toDouble();
    return dflt;
}
QString firstStr(const std::vector<DxfPair>& p, int code, const QString& dflt = QString()) {
    for (const auto& q : p) if (q.code == code) return q.value;
    return dflt;
}
bool hasCode(const std::vector<DxfPair>& p, int code) {
    for (const auto& q : p) if (q.code == code) return true;
    return false;
}

} // namespace

bool fromDxf(const QByteArray& data, std::vector<Element>& out, QString* err, DxfImportStats* stats) {
    if (err) err->clear();
    if (stats) *stats = DxfImportStats();
    // 先按文件声明的代码页/内容判定解码：我们的写出端中文用 GBK（ANSI_936），
    // 英文/纯 ASCII 用 ANSI_1252，别家 CAD 也可能给 UTF-8。
    const std::vector<DxfPair> tok = tokenizeDxf(decodeDxfBytes(data));
    if (tok.empty()) { if (err) *err = QStringLiteral("文件为空或不是 DXF 文本"); return false; }

    // 定位 ENTITIES 段
    size_t start = tok.size();
    for (size_t i = 0; i + 1 < tok.size(); ++i) {
        if (tok[i].code == 0 && tok[i].value.trimmed().compare("SECTION", Qt::CaseInsensitive) == 0 &&
            tok[i + 1].code == 2 && tok[i + 1].value.trimmed().compare("ENTITIES", Qt::CaseInsensitive) == 0) {
            start = i + 2;
            break;
        }
    }
    if (start >= tok.size()) { if (err) *err = QStringLiteral("未找到 ENTITIES 段"); return false; }

    // 通用样式套用
    const auto style = [](Element& e, const std::vector<DxfPair>& p) {
        const QString ly = firstStr(p, 8).trimmed();
        if (!ly.isEmpty()) e.layer = ly;
        if (hasCode(p, 62)) e.color = rgbFromAci((int)firstNum(p, 62, 7));
        const double lw100 = firstNum(p, 370, 13.0);   // 1/100 mm
        e.lineWidth = std::max(1.0, std::round(lw100 / 13.0));
        e.lineWidthAuto = false;
        e.lineStyle = LineStyle::Solid;
    };

    int count = 0;
    int arcsKept = 0, arcsDropped = 0;
    size_t i = start;
    // 旧式 POLYLINE 的状态（VERTEX 逐点跟随，SEQEND 收尾）
    std::vector<Vec3> pending;
    std::vector<double> pendingBulge;      // 每个顶点的 bulge（组码 42），写在**段起点**
    std::vector<DxfPair> pendingProps;
    bool pendingClosed = false, inPoly = false;

    // bulge（凸度）→ arcFlags。这是本函数唯一需要「两种参数化之间换算」的地方：
    //   外部 CAD：段由「起点 + 凸度 bulge」定义，bulge = tan(扫掠角/4)，可表达任意圆弧；
    //   本模型  ：段的圆由「三相邻点外接圆」决定（geo::arcThrough），是另一套参数化。
    // 因此只有二者一致时才敢标成弧段；不一致（幅值或方向对不上）就退回直线段——
    // 宁可少画一段弧，也绝不画出方向错误的圆弧（那比直线更糟）。
    // 一致的判据：由 bulge 反算的扫掠角 与 三相邻点定圆给出的扫掠角 之差 < 0.5°。
    const auto makeArcFlags = [&](const std::vector<Vec3>& pts, const std::vector<double>& bulges,
                                  bool closed) {
        const int n = (int)pts.size();
        const int nSegs = closed ? n : n - 1;
        std::vector<bool> flags((size_t)std::max(0, nSegs), false);
        for (int k = 0; k < nSegs && k < (int)bulges.size(); ++k) {
            const double b = bulges[(size_t)k];
            if (std::fabs(b) <= 1e-12) continue;
            const double wantDeg = 4.0 * std::atan(b) * 180.0 / M_PI;   // bulge 的反函数
            const geo::ArcSeg as = geo::arcThrough(pts, k, closed);
            if (as.valid && std::fabs(as.sweepDeg - wantDeg) < 0.5) { flags[(size_t)k] = true; ++arcsKept; }
            else ++arcsDropped;
        }
        return flags;
    };

    const auto flushPolyline = [&]() {
        if (inPoly && pending.size() >= 2) {
            Element e; e.type = ElementType::Polyline;
            e.pts = pending; e.closed = pendingClosed;
            e.arcFlags = makeArcFlags(pending, pendingBulge, pendingClosed);
            e.name = QStringLiteral("导入:折线");
            style(e, pendingProps);
            out.push_back(std::move(e));
            ++count;
        }
        pending.clear(); pendingBulge.clear(); pendingProps.clear(); pendingClosed = false; inPoly = false;
    };

    while (i < tok.size()) {
        if (tok[i].code != 0) { ++i; continue; }
        const QString t = tok[i].value.trimmed().toUpper();
        if (t == "ENDSEC" || t == "EOF") { flushPolyline(); break; }

        std::vector<DxfPair> props;
        size_t j = i + 1;
        while (j < tok.size() && tok[j].code != 0) { props.push_back(tok[j]); ++j; }

        if (t == "VERTEX") {
            if (inPoly) {
                pending.push_back(Vec3(firstNum(props, 10), firstNum(props, 20), 0));
                pendingBulge.push_back(firstNum(props, 42, 0.0));   // 该顶点出发那一段的凸度
            }
        } else if (t == "POLYLINE") {
            flushPolyline();
            inPoly = true;
            pendingClosed = ((int)firstNum(props, 70, 0) & 1) != 0;
            pendingProps = props;
        } else if (t == "SEQEND") {
            flushPolyline();
        } else if (t == "LINE") {
            Element e; e.type = ElementType::Line;
            e.pts = {Vec3(firstNum(props, 10), firstNum(props, 20), 0),
                     Vec3(firstNum(props, 11), firstNum(props, 21), 0)};
            e.name = QStringLiteral("导入:直线");
            style(e, props);
            out.push_back(std::move(e)); ++count;
        } else if (t == "CIRCLE") {
            Element e; e.type = ElementType::Circle;
            e.pts = {Vec3(firstNum(props, 10), firstNum(props, 20), 0)};
            e.radius = firstNum(props, 40);
            e.name = QStringLiteral("导入:圆");
            style(e, props);
            out.push_back(std::move(e)); ++count;
        } else if (t == "ARC") {
            Element e; e.type = ElementType::Arc;
            e.pts = {Vec3(firstNum(props, 10), firstNum(props, 20), 0)};
            e.radius = firstNum(props, 40);
            e.startAngle = firstNum(props, 50);
            e.endAngle = firstNum(props, 51);
            e.name = QStringLiteral("导入:圆弧");
            style(e, props);
            out.push_back(std::move(e)); ++count;
        } else if (t == "LWPOLYLINE") {
            // LWPOLYLINE（R13+ 实体）的顶点是「10/20 成对 + 可选 42 凸度」顺序出现，
            // 故 42 出现在两点之间时归属**上一个刚完成的顶点**（与 POLYLINE 同语义：
            // bulge 写在段起点上）。
            std::vector<Vec3> v;
            std::vector<double> bulges;
            double lx = 0, ly = 0;
            bool haveX = false;
            for (const auto& q : props) {
                if (q.code == 10) { lx = q.value.trimmed().toDouble(); haveX = true; }
                else if (q.code == 20) {
                    ly = q.value.trimmed().toDouble();
                    if (haveX) { v.push_back(Vec3(lx, ly, 0)); bulges.push_back(0.0); haveX = false; }
                } else if (q.code == 42 && !bulges.empty()) {
                    bulges.back() = q.value.trimmed().toDouble();
                }
            }
            if (v.size() >= 2) {
                Element e; e.type = ElementType::Polyline;
                e.pts = v;
                e.closed = ((int)firstNum(props, 70, 0) & 1) != 0;
                e.arcFlags = makeArcFlags(e.pts, bulges, e.closed);
                e.name = QStringLiteral("导入:多段线");
                style(e, props);
                out.push_back(std::move(e)); ++count;
            }
        } else if (t == "TEXT" || t == "MTEXT") {
            const QString s = firstStr(props, 1).trimmed();
            if (!s.isEmpty()) {
                Element e; e.type = ElementType::Text;
                e.pts = {Vec3(firstNum(props, 10), firstNum(props, 20), 0)};
                e.text = s;
                e.radius = firstNum(props, 40, 3.5);
                e.name = QStringLiteral("导入:文本");
                style(e, props);
                out.push_back(std::move(e)); ++count;
            }
        }
        // SOLID / POINT / 其它实体：跳过（本程序无对应图元类型）
        i = j;
    }

    if (count == 0) {
        if (err) *err = QStringLiteral("ENTITIES 段里没有可识别的图元");
        return false;
    }
    if (stats) {
        stats->entities    = count;
        stats->arcsKept    = arcsKept;
        stats->arcsDropped = arcsDropped;
    }
    return true;
}

} // namespace cadfmt
} // namespace SketchCAD
