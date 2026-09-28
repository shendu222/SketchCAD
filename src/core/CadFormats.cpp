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

bool canImport(CadFormat f) { return f == CadFormat::Json || f == CadFormat::Dxf || f == CadFormat::Dwg; }

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
        } else if (e.type == ElementType::Xline && !e.pts.empty()) {
            // R12 DXF 无 XLINE 原生实体：降级为通过点沿方向 ±10m 的长线段
            const double rad = e.rotation * M_PI / 180.0;
            const Vec3 d(std::cos(rad), std::sin(rad), 0);
            const Vec3 a(e.pts[0].x - d.x * 1e4, e.pts[0].y - d.y * 1e4, 0);
            const Vec3 b(e.pts[0].x + d.x * 1e4, e.pts[0].y + d.y * 1e4, 0);
            ent(QStringLiteral("LINE"), e);
            L << "10" << num(a.x) << "20" << num(a.y) << "30" << "0.0"
              << "11" << num(b.x) << "21" << num(b.y) << "31" << "0.0";
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

// =====================================================================
//  DWG R12 (AC1009) 原生二进制读 / 写
// =====================================================================
// 设计：DWG 是 Autodesk 私有二进制格式。本项目不引入 ODA/LibreDWG 等第三方库，
// 这里手写最广泛兼容的 R12 (AC1009) 子集：
//   写：图元炸开为 LINE/CIRCLE/ARC/TEXT/SOLID/POLYLINE+VERTEX（与 DXF 导出同一套
//       基元展开），封装进原生 R12 二进制容器（文件头 + 图层表 + 实体记录），
//       每个实体记录附带 CRC16（多项式 0x8408 反射，初值 0，与 LibreDWG 一致）。
//   读：解析 R12 二进制容器，还原 Element 列表供程序渲染。
// 兼容性：R12 是 AutoCAD / LibreCAD / QCAD / BricsCAD 等均可读取的最古老也最稳妥的
// DWG 版本。沙箱无法运行真实 CAD，故以"程序内往返自洽 + 独立 Python 解码器交叉验证"
// 为验收；个别 CAD 若有识别问题，可据其报错迭代。
// =====================================================================
namespace {

// 容器布局常量（编码器 / 解码器共用，保证自洽）
const int kDwgHeaderSize   = 22;                 // "AC1009" + 12 字节 image + 0x1A 0x00 0x00 0x00
const int kDwgPreTable     = kDwgHeaderSize + 4 + 4 + 4 + 4 + 4 + 4; // 头 + pEntities/pEntEnd/pBlockSec/pad/pBSend/pad
const int kDwgTableCount   = 10;                // block,layer,style,ltype,view,ucs,vport,appid,dimstyle,p13
const int kDwgTableposSize = 10;                // word size + u32 nr + u32 start

struct DwgRec {
    int kind = 0;            // 1 LINE,3 CIRCLE,7 TEXT,8 ARC,11 SOLID,19 PLINE,20 VERTEX,17 SEQEND
    int layerIdx = 0;
    int color = 7;           // ACI 颜色号
    int opts = 0;            // 实体选项位（见各类型定义）
    double x0=0,y0=0,z0=0, x1=0,y1=0,z1=0, x2=0,y2=0,z2=0, x3=0,y3=0,z3=0;
    double r=0, a0=0, a1=0, h=0, rot=0;
    QString text;
    bool closed = false;     // POLYLINE/VERTEX 闭合标志
};

void dwgPutU16(QByteArray& b, uint16_t v){ b.append((char)(v&0xff)); b.append((char)((v>>8)&0xff)); }
void dwgPutU32(QByteArray& b, uint32_t v){ b.append((char)(v&0xff)); b.append((char)((v>>8)&0xff)); b.append((char)((v>>16)&0xff)); b.append((char)((v>>24)&0xff)); }
void dwgPutDouble(QByteArray& b, double v){
    uint64_t u=0; ::memcpy(&u,&v,sizeof(double));
    for(int i=0;i<8;i++) b.append((char)((u>>(8*i))&0xff));
}
uint16_t dwgR16(const uint8_t* p){ return (uint16_t)(p[0] | (p[1]<<8)); }
uint32_t dwgR32(const uint8_t* p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }
double dwgRD(const uint8_t* p){ uint64_t u=0; for(int i=0;i<8;i++) u |= ((uint64_t)p[i])<<(8*i); double v; ::memcpy(&v,&u,sizeof(double)); return v; }

// CRC16 (DWG)：多项式 0x8408 反射，初值 0
uint16_t dwgCrc(const uint8_t* p, int len){
    static uint16_t tbl[256]; static bool init=false;
    if(!init){ for(int i=0;i<256;i++){ uint32_t j=i,c=0; for(int k=0;k<8;k++){ if((c^j)&1) c=(c>>1)^0x8408; else c>>=1; j>>=1; } tbl[i]=(uint16_t)c; } init=true; }
    uint16_t dx=0; for(int i=0;i<len;i++) dx=(uint16_t)((dx>>8)^tbl[(dx^p[i])&0xff]); return dx;
}

// 写一个实体记录（含 kind/flag/length/layer/opts/color + 字段 + CRC16）
void dwgAppendRec(QByteArray& out, const DwgRec& r){
    if(r.kind==17){ // SEQEND：无颜色，含 4 字节前驱指针占位
        QByteArray rec; rec.append((char)17); rec.append((char)0);
        rec.append((char)0); rec.append((char)0);          // length 占位
        dwgPutU16(rec,0); dwgPutU16(rec,0);                // layer,opts
        dwgPutU32(rec,0);                                  // 前驱指针占位
        int len=rec.size()+2; rec[2]=(char)(len&0xff); rec[3]=(char)((len>>8)&0xff);
        dwgPutU16(rec, dwgCrc((const uint8_t*)rec.constData(), rec.size()));
        out.append(rec); return;
    }
    QByteArray f;
    switch(r.kind){
        case 1: // LINE：2×3D 点
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0);dwgPutDouble(f,r.z0);
            dwgPutDouble(f,r.x1);dwgPutDouble(f,r.y1);dwgPutDouble(f,r.z1); break;
        case 3: // CIRCLE：2D 点 + 半径
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0);dwgPutDouble(f,r.r); break;
        case 8: // ARC：2D 点 + 半径 + 起止角
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0);dwgPutDouble(f,r.r);
            dwgPutDouble(f,r.a0);dwgPutDouble(f,r.a1); break;
        case 7: // TEXT：2D 点 + 字高 + 字符串 + [旋转]
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0);dwgPutDouble(f,r.h);
            { QByteArray a=r.text.toUtf8(); dwgPutU16(f,(uint16_t)a.size()); f.append(a); }
            if(r.opts&0x01) dwgPutDouble(f,r.rot);
            break;
        case 11: // SOLID：4×2D 点
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0);
            dwgPutDouble(f,r.x1);dwgPutDouble(f,r.y1);
            dwgPutDouble(f,r.x2);dwgPutDouble(f,r.y2);
            dwgPutDouble(f,r.x3);dwgPutDouble(f,r.y3); break;
        case 19: // POLYLINE：opts&1 → l70(闭合标志)
            if(r.opts&0x01) f.append((char)(r.closed?1:0));
            break;
        case 20: // VERTEX：2D 点
            dwgPutDouble(f,r.x0);dwgPutDouble(f,r.y0); break;
    }
    QByteArray rec;
    rec.append((char)r.kind);
    rec.append((char)0x01);                              // flag：含 ACI 颜色
    rec.append((char)0); rec.append((char)0);            // length 占位
    dwgPutU16(rec,(uint16_t)r.layerIdx);
    dwgPutU16(rec,(uint16_t)r.opts);
    rec.append((char)r.color);
    rec.append(f);
    int len=rec.size()+2; rec[2]=(char)(len&0xff); rec[3]=(char)((len>>8)&0xff);
    dwgPutU16(rec, dwgCrc((const uint8_t*)rec.constData(), rec.size()));
    out.append(rec);
}

// ---- 基元 → DwgRec 构建助手 ----
void dwgAddLineR(std::vector<DwgRec>& R,const Vec3&a,const Vec3&b,int layer,int color){
    DwgRec r; r.kind=1; r.layerIdx=layer; r.color=color;
    r.x0=a.x;r.y0=a.y;r.z0=0; r.x1=b.x;r.y1=b.y;r.z1=0; R.push_back(r);
}
void dwgAddCircleR(std::vector<DwgRec>& R,const Vec3&c,double rad,int layer,int color){
    DwgRec r; r.kind=3; r.layerIdx=layer; r.color=color; r.x0=c.x; r.y0=c.y; r.r=rad; R.push_back(r);
}
void dwgAddArcR(std::vector<DwgRec>& R,const Vec3&c,double rad,double a0,double a1,int layer,int color){
    DwgRec r; r.kind=8; r.layerIdx=layer; r.color=color; r.x0=c.x; r.y0=c.y; r.r=rad; r.a0=a0; r.a1=a1; R.push_back(r);
}
void dwgAddTextR(std::vector<DwgRec>& R,const Vec3&p,double h,double rot,const QString& s,int layer,int color){
    DwgRec r; r.kind=7; r.layerIdx=layer; r.color=color; r.x0=p.x; r.y0=p.y; r.h=h; r.rot=rot; r.text=s;
    if(std::fabs(rot)>1e-9) r.opts|=0x01;
    R.push_back(r);
}
void dwgAddSolidR(std::vector<DwgRec>& R,const Vec3&a,const Vec3&b,const Vec3&c,const Vec3&d,int layer,int color){
    DwgRec r; r.kind=11; r.layerIdx=layer; r.color=color;
    r.x0=a.x;r.y0=a.y; r.x1=b.x;r.y1=b.y; r.x2=c.x;r.y2=c.y; r.x3=d.x;r.y3=d.y; R.push_back(r);
}
void dwgEmitPolyline(std::vector<DwgRec>& R,const std::vector<Vec3>& pts,bool closed,int layer,int color){
    if(pts.size()<2) return;
    DwgRec pl; pl.kind=19; pl.layerIdx=layer; pl.color=color; pl.closed=closed;
    if(closed) pl.opts|=0x01;
    R.push_back(pl);
    for(const auto& v: pts){ DwgRec vx; vx.kind=20; vx.layerIdx=layer; vx.color=color; vx.x0=v.x; vx.y0=v.y; R.push_back(vx); }
    DwgRec se; se.kind=17; se.layerIdx=layer; se.color=color; R.push_back(se);
}

} // namespace (DWG helpers)

QByteArray toDwg(const std::vector<Element>& elems, QString* err, double drawingScale) {
    if(err) err->clear();
    dim::Style dimSt; dimSt.drawingScale = (drawingScale>1e-12)?drawingScale:1.0;

    // 图层集合（稳定排序，保证索引一致）
    QMap<QString,int> layerIdx; int li=0;
    layerIdx.insert(QStringLiteral("0"), li++);
    for(const auto& e: elems){ const QString ly=dxfLayer(e); if(!layerIdx.contains(ly)) layerIdx.insert(ly, li++); }
    const int nLayers = li;

    // 构建实体记录
    std::vector<DwgRec> R;
    for(const auto& e: elems){
        const int layer = layerIdx.value(dxfLayer(e),0);
        const int color = aciFromRgb(e.color);
        switch(e.type){
            case ElementType::Line: if(e.pts.size()>=2) dwgAddLineR(R,e.pts[0],e.pts[1],layer,color); break;
            case ElementType::Circle: if(!e.pts.empty()) dwgAddCircleR(R,e.pts[0],e.radius,layer,color); break;
            case ElementType::Arc: if(!e.pts.empty()) dwgAddArcR(R,e.pts[0],e.radius,e.startAngle,e.endAngle,layer,color); break;
            case ElementType::Ellipse: dwgEmitPolyline(R, geo::sampleEllipse(e.pts[0],e.radius,e.radiusY,e.rotation,96), true, layer,color); break;
            case ElementType::Rectangle: { const double x0=e.pts[0].x,y0=e.pts[0].y,x1=e.pts[1].x,y1=e.pts[1].y;
                dwgEmitPolyline(R, {Vec3(x0,y0,0),Vec3(x1,y0,0),Vec3(x1,y1,0),Vec3(x0,y1,0)}, e.polyClosed(), layer,color); } break;
            case ElementType::Polygon: dwgEmitPolyline(R, e.pts, e.polyClosed(), layer,color); break;
            case ElementType::Polyline: dwgEmitPolyline(R, e.pts, e.closed, layer,color); break;
            case ElementType::Spline: dwgEmitPolyline(R, geo::sampleSpline(e.pts,16), false, layer,color); break;
            case ElementType::Dimension: {
                const dim::Geom g = dim::build(e, dimSt); if(!g.valid) break;
                for(const auto& s: g.ext)  dwgAddLineR(R, s.first, s.second, layer,color);
                for(const auto& s: g.line) dwgAddLineR(R, s.first, s.second, layer,color);
                for(size_t i=1;i<g.arcPoly.size();++i) dwgAddLineR(R, g.arcPoly[i-1], g.arcPoly[i], layer,color);
                const dim::Style st;
                for(const auto& ar: g.arrows){
                    const double rad=ar.dirDeg*M_PI/180.0; const double cx=std::cos(rad), sy=std::sin(rad);
                    const Vec3 back(ar.tip.x-cx*st.arrowLen, ar.tip.y-sy*st.arrowLen,0);
                    const Vec3 off(-sy*st.arrowWidth*0.5, cx*st.arrowWidth*0.5,0);
                    dwgAddSolidR(R, ar.tip, back+off, back-off, back-off, layer,color);
                }
                if(!g.label.isEmpty()) dwgAddTextR(R, g.textPos, st.textHeight, g.textRotDeg, g.label, layer,color);
            } break;
            case ElementType::Text: if(!e.pts.empty()&&!e.text.isEmpty()) dwgAddTextR(R, e.pts[0], e.radius>0.1?e.radius:3.5, 0.0, e.text, layer,color); break;
            case ElementType::Xline: {
                // R12 无 XLINE 原生实体：降级为通过点沿方向 ±10m 的长线段
                if(!e.pts.empty()){
                    const double rad=e.rotation*M_PI/180.0;
                    const Vec3 d(std::cos(rad),std::sin(rad),0);
                    const Vec3 a(e.pts[0].x-d.x*1e4, e.pts[0].y-d.y*1e4, 0);
                    const Vec3 b(e.pts[0].x+d.x*1e4, e.pts[0].y+d.y*1e4, 0);
                    dwgAddLineR(R,a,b,layer,color);
                }
            } break;
            case ElementType::Hatch:
            case ElementType::Boundary:
                // 剖面区域/自定义边界：以闭合折线导出外环，内孔作为独立闭合折线（不导出剖面线）。
                if (e.pts.size() >= 3) dwgEmitPolyline(R, e.pts, true, layer, color);
                for (const auto& h : e.holes)
                    if (h.size() >= 3) dwgEmitPolyline(R, h, true, layer, color);
                break;
            default: break;
        }
    }

    // ---- 二进制容器 ----
    QByteArray out;
    out.append("AC1009",6);
    out.append(QByteArray(12,0));          // image 区（12 字节）
    out.append((char)0x1A); out.append((char)0); out.append((char)0); out.append((char)0);
    const int offPEntities = out.size(); dwgPutU32(out,0);   // pEntities
    dwgPutU32(out,0);                                   // pEntEnd
    dwgPutU32(out,0);                                   // pBlockSec
    out.append(QByteArray(4,0));                        // pad
    dwgPutU32(out,0);                                   // pBSend
    out.append(QByteArray(4,0));                        // pad
    const int offTablepos = out.size();                 // = kDwgPreTable
    for(int t=0;t<kDwgTableCount;t++){ dwgPutU16(out,0); dwgPutU32(out,0); dwgPutU32(out,0); }

    // 图层表数据（layer 在表序索引 1）
    const int offLayerTable = out.size();
    for(int i=0;i<nLayers;i++){
        QByteArray rec;
        rec.append((char)0);                               // flag
        QByteArray nm = layerIdx.key(i).toUtf8(); if(nm.size()>31) nm=nm.left(31);
        QByteArray nm32(32,0); for(int k=0;k<nm.size();k++) nm32[k]=nm[k]; rec.append(nm32);
        dwgPutU16(rec,0); dwgPutU16(rec,7); dwgPutU16(rec,0);   // used, color(ACI), style
        dwgPutU16(rec, dwgCrc((const uint8_t*)rec.constData(), rec.size())); // check_2
        out.append(rec);
    }
    {   // 图层表 check_32（32 字节，覆盖整表）
        uint16_t tc = dwgCrc((const uint8_t*)out.constData()+offLayerTable, out.size()-offLayerTable);
        QByteArray c32(32,0); c32[0]=(char)(tc&0xff); c32[1]=(char)((tc>>8)&0xff); out.append(c32);
    }
    {   // 头变量占位块（读者凭指针定位；这里写一段含图形范围的占位并补 check_32）
        QByteArray hv;
        dwgPutDouble(hv,0);dwgPutDouble(hv,0);dwgPutDouble(hv,0);          // inbase
        QRectF ext; bool has=false;
        for(const auto& e: elems){ const QRectF b=geo::elementBounds(e); ext=has?ext.united(b):b; has=true; }
        dwgPutDouble(hv, has?ext.left():0); dwgPutDouble(hv, has?ext.top():0); dwgPutDouble(hv,0);   // extmin
        dwgPutDouble(hv, has?ext.right():0);dwgPutDouble(hv, has?ext.bottom():0);dwgPutDouble(hv,0);  // extmax
        dwgPutDouble(hv,0);dwgPutDouble(hv,0);                                            // limmin
        dwgPutDouble(hv, has?ext.right():0);dwgPutDouble(hv, has?ext.bottom():0);          // limmax
        while(hv.size()<200) dwgPutDouble(hv,0);
        uint16_t hc = dwgCrc((const uint8_t*)hv.constData(), hv.size());
        QByteArray c32(32,0); c32[0]=(char)(hc&0xff); c32[1]=(char)((hc>>8)&0xff); hv.append(c32);
        out.append(hv);
    }

    // 实体段
    const int offEntities = out.size();
    for(const auto& r: R) dwgAppendRec(out, r);
    const int offEntEnd = out.size();

    // 回填指针 / 表定位
    auto patchU32 = [&](int off, uint32_t v){
        out[off]=(char)(v&0xff); out[off+1]=(char)((v>>8)&0xff); out[off+2]=(char)((v>>16)&0xff); out[off+3]=(char)((v>>24)&0xff);
    };
    patchU32(offPEntities, (uint32_t)offEntities);
    patchU32(offPEntities+4, (uint32_t)offEntEnd);
    const int offLayerNr   = offTablepos + 1*kDwgTableposSize + 2;        // size(2) 后
    const int offLayerStart= offTablepos + 1*kDwgTableposSize + 2 + 4;   // nr(4) 后
    patchU32(offLayerStart, (uint32_t)offLayerTable);
    out[offLayerNr]=(char)(nLayers&0xff); out[offLayerNr+1]=(char)((nLayers>>8)&0xff);
    out[offLayerNr+2]=(char)((nLayers>>16)&0xff); out[offLayerNr+3]=(char)((nLayers>>24)&0xff);

    return out;
}

bool fromDwg(const QByteArray& data, std::vector<Element>& out, QString* err, DxfImportStats* stats) {
    if(err) err->clear();
    if(stats) *stats = DxfImportStats();
    const uint8_t* d=(const uint8_t*)data.constData(); const int n=data.size();
    if(n < (int)(kDwgPreTable + kDwgTableCount*kDwgTableposSize)){ if(err)*err=QStringLiteral("文件过短或不是 DWG"); return false; }
    if(!(d[0]=='A'&&d[1]=='C'&&d[2]=='1'&&d[3]=='0'&&d[4]=='0'&&d[5]=='9')){ if(err)*err=QStringLiteral("不是 R12 DWG (AC1009)"); return false; }
    const uint32_t pEntities = dwgR32(d + kDwgHeaderSize);
    const uint32_t pEntEnd   = dwgR32(d + kDwgHeaderSize + 4);
    if(pEntities==0 || pEntEnd<=pEntities || (int)pEntEnd>n){ if(err)*err=QStringLiteral("实体段指针无效"); return false; }

    // 图层表：layer 在表序索引 1
    QMap<int,QString> layerNames; layerNames[0]=QStringLiteral("0");
    const int offLayerNr    = kDwgPreTable + 1*kDwgTableposSize + 2;
    const int offLayerStart = kDwgPreTable + 1*kDwgTableposSize + 6;
    const int nLayers = (int)dwgR32(d + offLayerNr);
    const uint32_t layerStart = dwgR32(d + offLayerStart);
    if(layerStart>0 && (int)layerStart<n){
        int p = (int)layerStart;
        for(int i=0;i<nLayers && p+44<=n;i++){
            p+=1;                                          // flag
            QByteArray nm((const char*)d+p, 32); p+=32;
            const QString name = QString::fromUtf8(nm).trimmed(); p+=2+2+2+2; // used,color,style,check_2
            layerNames[i] = name.isEmpty()?QStringLiteral("0"):name;
        }
    }

    int count=0; int i=(int)pEntities;
    std::vector<Vec3> polyPts; bool inPoly=false; bool polyClosed=false;
    while(i < (int)pEntEnd){
        const uint8_t kind=d[i], flag=d[i+1];
        if(i+4>n) break;
        const uint16_t len = dwgR16(d+i+2);
        if(len<10 || i+len>n) break;
        const uint16_t layer = dwgR16(d+i+4);
        const uint16_t opts  = dwgR16(d+i+6);
        int p = i+8;
        int color = 7;
        if(flag&0x01){ color=d[p]; p+=1; }
        const QString ly = layerNames.value((int)layer, QStringLiteral("0"));

        Element el; bool emitEl=false;
        switch(kind){
            case 1: if(p+48<=n){ double x0=dwgRD(d+p),y0=dwgRD(d+p+8); p+=24; double x1=dwgRD(d+p),y1=dwgRD(d+p+8);
                        el.type=ElementType::Line; el.pts={Vec3(x0,y0,0),Vec3(x1,y1,0)}; emitEl=true; } break;
            case 3: if(p+24<=n){ double cx=dwgRD(d+p),cy=dwgRD(d+p+8),r=dwgRD(d+p+16);
                        el.type=ElementType::Circle; el.pts={Vec3(cx,cy,0)}; el.radius=r; emitEl=true; } break;
            case 8: if(p+40<=n){ double cx=dwgRD(d+p),cy=dwgRD(d+p+8),r=dwgRD(d+p+16);
                        double a0=dwgRD(d+p+24),a1=dwgRD(d+p+32);
                        el.type=ElementType::Arc; el.pts={Vec3(cx,cy,0)}; el.radius=r; el.startAngle=a0; el.endAngle=a1; emitEl=true; } break;
            case 7: if(p+24<=n){ double x=dwgRD(d+p),y=dwgRD(d+p+8),h=dwgRD(d+p+16); p+=24;
                        if(p+2<=n){ int tlen=(int)dwgR16(d+p); p+=2;
                            if(p+tlen<=n){ QString s=QString::fromUtf8((const char*)d+p,tlen); p+=tlen;
                                if(opts&0x01 && p+8<=n){ p+=8; }  // 旋转角（R12 文本，导入时暂不还原）
                                el.type=ElementType::Text; el.pts={Vec3(x,y,0)}; el.radius=h; el.text=s; emitEl=true; } } } break;
            case 11: if(p+64<=n){ Vec3 a(dwgRD(d+p),dwgRD(d+p+8),0); p+=16;
                        Vec3 b(dwgRD(d+p),dwgRD(d+p+8),0); p+=16;
                        Vec3 c(dwgRD(d+p),dwgRD(d+p+8),0); p+=16;
                        Vec3 dd(dwgRD(d+p),dwgRD(d+p+8),0);
                        el.type=ElementType::Polygon; el.pts={a,b,dd,c}; el.closed=true; emitEl=true; } break;
            case 19: polyPts.clear(); inPoly=true; polyClosed=(opts&0x01)!=0;
                        if(opts&0x01 && p+1<=n) p+=1;
                        break;
            case 20: if(p+16<=n){ double x=dwgRD(d+p),y=dwgRD(d+p+8); if(inPoly) polyPts.push_back(Vec3(x,y,0)); } break;
            case 17: if(inPoly){ el.type=ElementType::Polyline; el.pts=polyPts; el.closed=polyClosed; emitEl=true; inPoly=false; } break;
        }
        if(emitEl){
            el.layer = ly; el.color = rgbFromAci(color);
            el.name = QStringLiteral("导入:DWG");
            out.push_back(std::move(el)); ++count;
        }
        i += len;
    }
    if(count==0){ if(err)*err=QStringLiteral("未解析到任何图元"); return false; }
    if(stats) stats->entities = count;
    return true;
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
            case ElementType::Xline:
                // STEP 无 XLINE：降级为通过点沿方向 ±10m 的截断长线段
                if (!e.pts.empty()) {
                    const double rad = e.rotation * M_PI / 180.0;
                    const Vec3 d(std::cos(rad), std::sin(rad), 0);
                    addTrimLine(Vec3(e.pts[0].x - d.x * 1e4, e.pts[0].y - d.y * 1e4, 0),
                                Vec3(e.pts[0].x + d.x * 1e4, e.pts[0].y + d.y * 1e4, 0));
                }
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
            case ElementType::Hatch:
            case ElementType::Boundary:
                // 剖面区域/自定义边界：以闭合折线导出外环，内孔作为独立闭合折线（不导出剖面线）。
                if (e.pts.size() >= 3) addPoly(e.pts, true);
                for (const auto& h : e.holes) if (h.size() >= 3) addPoly(h, true);
                break;
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
