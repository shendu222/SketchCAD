#include "CadFormats.h"
#include "Geometry.h"
#include "DimStyle.h"
#include "TextStyle.h"   // resolveFields：导出前把 %<字段>% 占位解析为求值快照
#include "Block.h"       // 外部块表读取：block::blockDefFromJson / BlockDef
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStringList>
#include <QDateTime>
#include <QSet>
#include <QMap>
#include <map>
#include <cmath>
#include <algorithm>
#include <functional>

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

// 产品 9 种线型 → DXF LTYPE 名。命名与 fromDxf 侧 lineStyleFromDxfName 反向兼容：
// DASHED*/DOTTED/CENTER/PHANTOM/DASHDOT* 前缀均能映射回对应 LineStyle
// （Wave/DoubleBreak/Thick* 回读粗化为 Dashed/DashDot——R12 dash 语义无法表达波浪/
// 双折/粗细档，属有损映射，文档注明）。
static const char* dxfLtypeName(LineStyle ls) {
    switch (ls) {
        case LineStyle::Dashed:       return "DASHED";
        case LineStyle::Dotted:       return "DOTTED";
        case LineStyle::DashDot:      return "CENTER";
        case LineStyle::DashDotDot:   return "PHANTOM";
        case LineStyle::Wave:         return "DASHEDWAVE";
        case LineStyle::DoubleBreak:  return "DASHDOTDBRK";
        case LineStyle::ThickDashed:  return "DASHEDTHICK";
        case LineStyle::ThickDashDot: return "DASHDOTTHICK";
        case LineStyle::Solid:        break;
    }
    return "CONTINUOUS";
}

// 写一条 LTYPE 表条目（R12：72=65 对齐码恒 'A'、73=划数、40=总图案长、49=每划长度；
// 正值=落笔画线、负值=空移、0=点）。GB/T 4457.4 线素长度的近似值。
static void dxfWriteLtype(QStringList& L, const char* name, const char* desc,
                          const std::vector<double>& dashes) {
    double total = 0.0;
    for (double d : dashes) total += std::fabs(d);
    L << "0" << "LTYPE" << "2" << name << "70" << "64"
      << "3" << desc << "72" << "65"
      << "73" << QString::number((int)dashes.size())
      << "40" << QString::number(total, 'f', 6);
    for (double d : dashes) L << "49" << QString::number(d, 'f', 6);
}

// Dimension 12 类 → DXF DIMENSION 70 组码类型；-1 = R12 无对应原生表达（保留炸开）。
//   Distance/Horizontal/Vertical→0（旋转线性，50 写投影角）  Aligned/Baseline/Continue→1
//   Diameter→3  Radius→4  Angle→5；Arc/Leader/Ordinate 无原生组码语义 → 炸开。
static int dxfDimKind(DimensionType t) {
    switch (t) {
        case DimensionType::Distance:
        case DimensionType::Horizontal:
        case DimensionType::Vertical:  return 0;
        case DimensionType::Aligned:
        case DimensionType::Baseline:
        case DimensionType::Continue:  return 1;
        case DimensionType::Diameter:  return 3;
        case DimensionType::Radius:    return 4;
        case DimensionType::Angle:     return 5;
        default:                       return -1;
    }
}

QByteArray toDxf(const std::vector<Element>& elems, QString* err,
                 double drawingScale, int insunits,
                 const std::vector<BlockDef>* blocks, const DxfExportOptions* opts) {
    if (err) err->clear();
    const DxfExportOptions defOpt;                    // 默认值 = 旧炸开行为
    const DxfExportOptions& opt = opts ? *opts : defOpt;

    // 标注数字要还原成实物尺寸：把绘图比例交给 dim::build
    dim::Style dimSt;
    dimSt.drawingScale = (drawingScale > 1e-12) ? drawingScale : 1.0;

    QStringList L;       // 最终文件体（HEADER/TABLES/BLOCKS/ENTITIES/EOF 按序拼装）
    QStringList LEnt;    // ENTITIES 段先行写出（期间收集 *D 匿名块，BLOCKS 段最后拼装）
    // *D 匿名块队列：原生标注的显示几何（块名 → 块体内码，坐标已相对定义点）
    std::vector<std::pair<QString, QStringList>> anonDims;
    int anonSeq = 0;

    const auto num = [](double v) { return QString::number(v, 'f', 6); };

    // ---- 图形范围（供看图软件 zoom-extents）----
    // 原生模式下块引用不再炸开输出，但范围必须包含块内几何：用 expandRef 生成
    // 世界坐标展开副本参与包围盒计算（展开结果只算范围，不落盘）。
    // 旧调用（opts==nullptr）保持原逻辑：直接对 elems 计算，输出与升级前一致。
    std::vector<Element> boundsEls;
    if (opts != nullptr && blocks != nullptr) {
        boundsEls.reserve(elems.size());
        for (const auto& e : elems) {
            if (e.type == ElementType::BlockRef) {
                const auto subs = block::expandRef(e, *blocks);
                boundsEls.insert(boundsEls.end(), subs.begin(), subs.end());
            } else {
                boundsEls.push_back(e);
            }
        }
    } else {
        boundsEls = elems;
    }
    QRectF ext(0.0, 0.0, 0.0, 0.0);
    bool hasExt = false;
    for (const auto& e : boundsEls) {
        const QRectF b = geo::elementBounds(e);
        ext = hasExt ? ext.united(b) : b;
        hasExt = true;
    }
    const QString ymin = num(ext.top()), ymax = num(ext.bottom());

    // ---- 图层清单（块内图元图层并入，保证 LAYER 表覆盖所有引用）----
    QSet<QString> layers;
    layers.insert(QStringLiteral("0"));
    for (const auto& e : elems) layers.insert(dxfLayer(e));
    if (blocks)
        for (const auto& d : *blocks)
            for (const auto& e : d.entities) layers.insert(dxfLayer(e));

    // ---- HEADER ----
    // 显式声明版本是必须的：本文件用的是 R12 实体与组码，不声明版本解析器只能猜，
    // 猜错就会出现「实体与版本不自洽」的解析失败（例如 LWPOLYLINE 在 R12 里不存在）。
    L << "0" << "SECTION" << "2" << "HEADER"
      << "9" << "$ACADVER" << "1" << "AC1009"
      << "9" << "$DWGCODEPAGE" << "3" << "@@CP@@"
      // $INSUNITS：度量口径元数据（AutoCAD 标准码）。内部几何恒为 mm，此码只声明
      // 「文件里数值的口径」，导入方据此换算——是跨单位交换的语义锚点（见 Units.h）。
      << "9" << "$INSUNITS" << "70" << QString::number(insunits)
      << "9" << "$INSBASE" << "10" << "0.0" << "20" << "0.0" << "30" << "0.0"
      << "9" << "$EXTMIN"  << "10" << num(ext.left())  << "20" << ymin << "30" << "0.0"
      << "9" << "$EXTMAX"  << "10" << num(ext.right()) << "20" << ymax << "30" << "0.0"
      << "9" << "$LIMMIN"  << "10" << "0.0" << "20" << "0.0"
      << "9" << "$LIMMAX"  << "10" << num(ext.right()) << "20" << ymax
      << "0" << "ENDSEC";

    // ---- TABLES：线型表（产品 9 线型全定义）+ 图层表 + 文字样式表 + 标注样式表 ----
    // 注意：R12 没有真彩色(420) 与线宽(370) 组码，所以这里只登记 ACI 颜色，
    // 颜色信息经 62 组码近似到最近的 ACI 色号（本程序调色板本就是标准色，损失可忽略）。
    L << "0" << "SECTION" << "2" << "TABLES"
      << "0" << "TABLE" << "2" << "LTYPE" << "70" << "9";
    dxfWriteLtype(L, "CONTINUOUS",   "Solid line",             {});
    dxfWriteLtype(L, "DASHED",       "Dashed __ __ __",        {12.0, -3.0});
    dxfWriteLtype(L, "DOTTED",       "Dotted . . . .",         {0.0, -3.0});
    dxfWriteLtype(L, "CENTER",       "Center __ . __ .",       {30.0, -4.0, 0.0, -4.0});
    dxfWriteLtype(L, "PHANTOM",      "Phantom __ . . .",       {50.0, -5.0, 0.0, -5.0, 0.0, -5.0});
    dxfWriteLtype(L, "DASHEDWAVE",   "Wave (dash approx)",     {3.0, -2.0});
    dxfWriteLtype(L, "DASHDOTDBRK",  "Double break (approx)",  {30.0, -4.0, 0.0, -4.0});
    dxfWriteLtype(L, "DASHEDTHICK",  "Thick dashed",           {12.0, -3.0});
    dxfWriteLtype(L, "DASHDOTTHICK", "Thick dash-dot",         {30.0, -4.0, 0.0, -4.0});
    L << "0" << "ENDTAB"
      << "0" << "TABLE" << "2" << "LAYER" << "70" << QString::number(layers.size());
    for (const QString& ly : layers)
        L << "0" << "LAYER" << "2" << ly
          << "70" << "0" << "62" << "7" << "6" << "CONTINUOUS";
    L << "0" << "ENDTAB";
    // 文字样式表：TEXT 实体缺省指向 STANDARD，必须给出定义（R12 最小集：
    // 40=固定字高 0=不固定、41=宽度因子、50=倾角、71=生成标志、42=上次字高、3=字体文件）
    L << "0" << "TABLE" << "2" << "STYLE" << "70" << "1"
      << "0" << "STYLE" << "2" << "STANDARD" << "70" << "0"
      << "40" << "0.0" << "41" << "1.0" << "50" << "0.0" << "71" << "0"
      << "42" << "2.5" << "3" << "txt"
      << "0" << "ENDTAB";
    // 标注样式表（组码对照 paulbourke DXF R12 / Autodesk 官方参考）：
    // 40=DIMSCALE 41=DIMASZ 42=DIMEXO 43=DIMDLI 44=DIMEXE 140=DIMTXT 141=DIMCEN
    // 142=DIMTSZ 144=DIMLFAC 147=DIMGAP 77=DIMTAD 78=DIMZIN。
    // DIMLFAC=1/绘图比例：AutoCAD 测得图上距离 × DIMLFAC = 实物尺寸
    // （与 dim::build 的 value = measured / drawingScale 同一语义，regen 后数值正确）。
    const double dimlfac = (dimSt.drawingScale > 1e-12) ? 1.0 / dimSt.drawingScale : 1.0;
    L << "0" << "TABLE" << "2" << "DIMSTYLE" << "70" << "1"
      << "0" << "DIMSTYLE" << "2" << "STANDARD" << "70" << "0"
      << "40" << "1.0"                       // DIMSCALE 全局比例
      << "41" << "3.5"                       // DIMASZ  箭头大小
      << "42" << "0.0"                       // DIMEXO  界线起点偏移
      << "43" << "10.0"                      // DIMDLI  基线间距
      << "44" << "2.5"                       // DIMEXE  界线超出量
      << "140" << "3.5"                      // DIMTXT  标注字高
      << "141" << "0.0"                      // DIMCEN  圆心标记（关）
      << "142" << "0.0"                      // DIMTSZ  斜线尺寸（0=箭头）
      << "144" << num(dimlfac)               // DIMLFAC 线性测量比例因子
      << "147" << "0.8"                      // DIMGAP  文字与尺寸线间距
      << "77" << "1"                         // DIMTAD  文字置于尺寸线上方（GB）
      << "78" << "0"                         // DIMZIN  零抑制（不抑制）
      << "0" << "ENDTAB" << "0" << "ENDSEC";

    // ============ 实体段（先行写出；期间收集 *D 匿名块，BLOCKS 段最后拼装）============
    LEnt << "0" << "SECTION" << "2" << "ENTITIES";

    // 实体公共头：图层 + ACI 颜色 + 线型（非实线带出 6 组码，LTYPE 表已定义同名线型）
    const auto ent = [&](QStringList& T, const QString& type, const Element& e) {
        T << "0" << type << "8" << dxfLayer(e) << "62" << QString::number(aciFromRgb(e.color));
        if (e.lineStyle != LineStyle::Solid)
            T << "6" << dxfLtypeName(e.lineStyle);
    };

    // 多段线：R12 用 POLYLINE/VERTEX/SEQEND（LWPOLYLINE 是 R13+ 实体，不能出现在 R12 文件里）。
    // arcFlags[i]=true 表示 pts[i]→pts[i+1] 为圆弧段，用 VERTEX 的 42 组码（bulge=tan(含角/4)）表达，
    // 保留真实弧段而不是离散成折线。
    const auto polylineEx = [&](QStringList& T, const Element& e, const std::vector<Vec3>& pts,
                               const std::vector<bool>& arcFlags, bool closed) {
        if (pts.size() < 2) return;
        ent(T, QStringLiteral("POLYLINE"), e);
        T << "66" << "1" << "70" << (closed ? "1" : "0");
        const int segCount = closed ? (int)pts.size() : (int)pts.size() - 1;
        for (int i = 0; i < (int)pts.size(); ++i) {
            T << "0" << "VERTEX" << "8" << dxfLayer(e)
              << "10" << num(pts[i].x) << "20" << num(pts[i].y);
            if (i < (int)arcFlags.size() && arcFlags[i] && i < segCount) {
                const geo::ArcSeg as = geo::arcThrough(pts, i, closed);
                if (as.valid && as.radius > 1e-12) {
                    const double bulge = std::tan(as.sweepDeg * M_PI / 180.0 / 4.0);
                    if (std::fabs(bulge) > 1e-12) T << "42" << QString::number(bulge, 'f', 9);
                }
            }
        }
        T << "0" << "SEQEND" << "8" << dxfLayer(e);
    };
    const auto polyline = [&](QStringList& T, const Element& e,
                              const std::vector<Vec3>& pts, bool closed) {
        polylineEx(T, e, pts, std::vector<bool>(), closed);
    };

    // 文本：把多行内容按行拆成多个 TEXT 实体。
    // 绝不能把含 \n 的字符串整段写进组码 1 —— 那会把值行劈成两行，后续组码全部错位，
    // 整份 DXF 直接不可解析（标注数字就是 "xx m\n(xx ft)" 这种双行文本）。
    const auto textLines = [&](QStringList& T, const Element& e, const QString& raw, const Vec3& pos,
                               double h, double rot, bool centered, double wf = 1.0) {
        QString t = raw;
        t.replace("\r\n", "\n");
        t.replace('\r', '\n');
        const QStringList parts = t.split('\n');
        const double lineH = (h > 0.1 ? h : 3.5) * 1.3;
        for (int i = 0; i < parts.size(); ++i) {
            const QString line = dxfSafe(parts.at(i));
            if (line.trimmed().isEmpty()) continue;
            const double y = pos.y - i * lineH;
            ent(T, QStringLiteral("TEXT"), e);
            T << "10" << num(pos.x) << "20" << num(y) << "40" << num(h);
            if (std::fabs(wf - 1.0) > 1e-9) T << "41" << num(wf);   // 宽度因子（MTEXT）
            if (std::fabs(rot) > 1e-9) T << "50" << num(rot);
            if (centered) T << "72" << "1" << "73" << "2" << "11" << num(pos.x) << "21" << num(y);
            T << "1" << line;
        }
    };

    // 实心箭头（SOLID 三角形：第 4 点与第 3 点重合退化为三角形——DXF 标准写法）。
    // 尺寸由 dimSt（arrowLen/arrowWidth）决定，原生 *D 块与降级炸开共用同一几何。
    const auto arrowSolid = [&](QStringList& T, const Element& e, const dim::Arrow& ar) {
        const double rad = ar.dirDeg * M_PI / 180.0;
        const double cx = std::cos(rad), sy = std::sin(rad);
        const Vec3 back(ar.tip.x - cx * dimSt.arrowLen, ar.tip.y - sy * dimSt.arrowLen, 0);
        const Vec3 off(-sy * dimSt.arrowWidth * 0.5, cx * dimSt.arrowWidth * 0.5, 0);
        const Vec3 p1 = back + off, p2 = back - off;
        ent(T, QStringLiteral("SOLID"), e);
        T << "10" << num(ar.tip.x) << "20" << num(ar.tip.y)
          << "11" << num(p1.x) << "21" << num(p1.y)
          << "12" << num(p2.x) << "22" << num(p2.y)
          << "13" << num(p2.x) << "23" << num(p2.y);
    };

    // ---- 实体写出（含块/标注/填充原生分支；递归支持块内嵌套块引用，depth<8 防环）----
    std::function<void(const Element&, QStringList&, int)> writeEntity =
        [&](const Element& e, QStringList& T, int depth) {
        if (e.type == ElementType::Line && e.pts.size() >= 2) {
            ent(T, QStringLiteral("LINE"), e);
            T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "11" << num(e.pts[1].x) << "21" << num(e.pts[1].y) << "31" << "0.0";
        } else if (e.type == ElementType::Circle && !e.pts.empty()) {
            ent(T, QStringLiteral("CIRCLE"), e);
            T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "40" << num(e.radius);
        } else if (e.type == ElementType::Arc && !e.pts.empty()) {
            ent(T, QStringLiteral("ARC"), e);
            T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
              << "40" << num(e.radius)
              << "50" << num(e.startAngle) << "51" << num(e.endAngle);
        } else if (e.type == ElementType::Ellipse && !e.pts.empty()) {
            // R12 无原生 ELLIPSE 实体（R13+ 才有），按椭圆母线采样成闭合多段线；
            // 段数经 DxfExportOptions::ellipseSegments 配置（默认 96，与旧版一致）
            polyline(T, e, geo::sampleEllipse(e.pts[0], e.radius, e.radiusY, e.rotation,
                     opt.ellipseSegments > 8 ? opt.ellipseSegments : 96), true);
        } else if (e.type == ElementType::Rectangle && e.pts.size() >= 2) {
            const double x0 = e.pts[0].x, y0 = e.pts[0].y, x1 = e.pts[1].x, y1 = e.pts[1].y;
            polyline(T, e, {Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0)},
                     e.polyClosed());
        } else if (e.type == ElementType::Polygon && e.pts.size() >= 2) {
            polyline(T, e, e.pts, e.polyClosed());   // 剪切过的轮廓导成开放多段线
        } else if (e.type == ElementType::Polyline && e.pts.size() >= 2) {
            polylineEx(T, e, e.pts, e.arcFlags, e.closed);
        } else if (e.type == ElementType::Dimension) {
            const dim::Geom g = dim::build(e, dimSt);
            if (!g.valid) return;
            const int kind = dxfDimKind(e.dtype);
            // 原生 DIMENSION：仅顶层（块内标注炸开，避免依赖块内 DIMSTYLE 上下文）
            // 且类型可映射（9 类）；Arc/Leader/Ordinate 3 类无原生组码语义，保留炸开。
            if (opts && opt.nativeDims && depth == 0 && kind >= 0) {
                // ---- 定义点与附加点（组码语义对照 R12 DIMENSION）----
                Vec3 defP;                          // 10/20：线性=尺寸线定位点；径向=弧上定义点
                Vec3 p13, p14; bool has1314 = false;
                Vec3 p15;      bool has15 = false;
                double rotDeg = 0.0;
                if (kind == 0) {
                    has1314 = true; p13 = e.pts[0]; p14 = e.pts[1];
                    // 产品语义：Distance 自动取投影较长方向；Horizontal/Vertical 显式指定。
                    // 50 组码写入后，导入端按该方向投影即还原原值。
                    if      (e.dtype == DimensionType::Horizontal) rotDeg = 0.0;
                    else if (e.dtype == DimensionType::Vertical)   rotDeg = 90.0;
                    else rotDeg = (std::fabs(e.pts[1].x - e.pts[0].x) <
                                   std::fabs(e.pts[1].y - e.pts[0].y)) ? 90.0 : 0.0;
                    defP = g.line.empty() ? g.textPos
                        : Vec3((g.line[0].first.x + g.line[0].second.x) * 0.5,
                               (g.line[0].first.y + g.line[0].second.y) * 0.5, 0);
                } else if (kind == 1) {          // 对齐/基线/连续：13/14=两定义点
                    has1314 = true; p13 = e.pts[0]; p14 = e.pts[1];
                    defP = g.line.empty() ? g.textPos
                        : Vec3((g.line[0].first.x + g.line[0].second.x) * 0.5,
                               (g.line[0].first.y + g.line[0].second.y) * 0.5, 0);
                } else if (kind == 5) {          // 角度：15=顶点、13/14=两脚
                    has15 = true; has1314 = true;
                    p15 = e.pts[0]; p13 = e.pts[1]; p14 = e.pts[2];
                    defP = g.arcPoly.size() >= 2 ? g.arcPoly[g.arcPoly.size() / 2] : g.textPos;
                } else {                          // 半径(4)/直径(3)：15=圆心、10=弧上定义点
                    has15 = true; p15 = e.pts[0];
                    defP = e.pts[1];
                }
                // ---- *D 匿名块：标注显示几何。块内坐标=世界-定义点、BLOCK 基点=0：
                //      无论读取方按「相对定义点偏移」（AutoCAD 语义）还是「普通 INSERT
                //      at defP」（平移 = defP - base）解释，都还原为同一世界坐标。
                const QString aname = QStringLiteral("*D%1").arg(++anonSeq);
                QStringList BD;
                const auto rel = [&](const Vec3& p) { return Vec3(p.x - defP.x, p.y - defP.y, 0); };
                const auto seg = [&](const Vec3& a, const Vec3& b) {
                    const Vec3 ra = rel(a), rb = rel(b);
                    ent(BD, QStringLiteral("LINE"), e);
                    BD << "10" << num(ra.x) << "20" << num(ra.y) << "30" << "0.0"
                       << "11" << num(rb.x) << "21" << num(rb.y) << "31" << "0.0";
                };
                for (const auto& s : g.ext)  seg(s.first, s.second);
                for (const auto& s : g.line) seg(s.first, s.second);
                if (g.arcPoly.size() >= 2) {
                    std::vector<Vec3> ap;
                    ap.reserve(g.arcPoly.size());
                    for (const auto& p : g.arcPoly) ap.push_back(rel(p));
                    Element pe;                    // 块内折线代理（样式继承标注）
                    pe.type = ElementType::Polyline;
                    pe.layer = e.layer; pe.color = e.color; pe.lineStyle = e.lineStyle;
                    polylineEx(BD, pe, ap, std::vector<bool>(), false);
                }
                for (const auto& ar : g.arrows) {
                    dim::Arrow ra = ar;
                    ra.tip = rel(ar.tip);
                    arrowSolid(BD, e, ra);
                }
                if (!g.label.isEmpty())
                    textLines(BD, e, g.label, rel(g.textPos), dimSt.textHeight, g.textRotDeg, true);
                anonDims.push_back({aname, BD});
                // ---- DIMENSION 实体本体 ----
                ent(T, QStringLiteral("DIMENSION"), e);
                T << "2" << aname
                  << "10" << num(defP.x) << "20" << num(defP.y) << "30" << "0.0"
                  << "11" << num(g.textPos.x) << "21" << num(g.textPos.y) << "31" << "0.0"
                  << "70" << QString::number(kind);
                if (kind == 0) T << "50" << num(rotDeg);
                if (has1314)
                    T << "13" << num(p13.x) << "23" << num(p13.y) << "33" << "0.0"
                      << "14" << num(p14.x) << "24" << num(p14.y) << "34" << "0.0";
                if (has15)
                    T << "15" << num(p15.x) << "25" << num(p15.y) << "35" << "0.0";
                if (!e.text.isEmpty()) T << "1" << dxfSafe(e.text);
                T << "3" << "STANDARD";
                return;
            }
            // 降级：按 GB 生成的几何「炸开」导出：尺寸线/界线用 LINE、箭头用实心 SOLID、
            // 数字用 TEXT。这样任何看图软件不依赖标注样式表也能显示成国标的样子。
            const auto seg = [&](const Vec3& a, const Vec3& b) {
                ent(T, QStringLiteral("LINE"), e);
                T << "10" << num(a.x) << "20" << num(a.y) << "30" << "0.0"
                  << "11" << num(b.x) << "21" << num(b.y) << "31" << "0.0";
            };
            for (const auto& s : g.ext)  seg(s.first, s.second);
            for (const auto& s : g.line) seg(s.first, s.second);
            for (size_t i = 1; i < g.arcPoly.size(); ++i) seg(g.arcPoly[i - 1], g.arcPoly[i]);
            for (const auto& ar : g.arrows) arrowSolid(T, e, ar);
            if (!g.label.isEmpty())
                textLines(T, e, g.label, g.textPos, dimSt.textHeight, g.textRotDeg, true);
        } else if (e.type == ElementType::Spline && e.pts.size() >= 2) {
            // 样条按屏幕上看到的那条 Catmull-Rom 曲线离散后导出。
            // 直接导原始控制点会让 DXF 里变成折线 —— 跟屏幕显示不一致，
            // 下游看到的形状就不是设计者画的那条曲线了。
            // R12 无 SPLINE 原生实体；段数经 DxfExportOptions::splineSegments 配置精度。
            polyline(T, e, geo::sampleSpline(e.pts,
                     opt.splineSegments > 2 ? opt.splineSegments : 16), false);
        } else if ((e.type == ElementType::Text || e.type == ElementType::MText)
                   && !e.pts.empty() && !e.text.isEmpty()) {
            // 文本：单行 Text 与多行 MText 统一按行拆成多个 TEXT 实体（R12 无 MTEXT 原生实体，
            // 多行 = 首锚点向下按 1.3 倍行距逐行展开，与画布渲染一致）。
            // 导出前解析 %<字段>% 占位（导出为求值后的静态快照）；MText 带出宽度因子、
            // 旋转角与对齐列（对齐列为中心列时用中间对齐 72/73，与 9 点对齐的列语义对应）。
            const bool isMT = (e.type == ElementType::MText);
            textLines(T, e, resolveFields(e.text, nullptr), e.pts[0],
                      e.radius > 0.1 ? e.radius : 3.5,
                      isMT ? e.rotation : 0.0,
                      isMT && (e.textAlign % 3 == 1),
                      isMT && e.widthFactor > 0 ? e.widthFactor : 1.0);
        } else if (e.type == ElementType::Table && !e.pts.empty()) {
            // 表格（明细表）：R12 无 TABLE 语义 → 炸开为 LINE 网格 + TEXT 单元格文字，
            // 网格几何与画布渲染同源（列宽累加、行高统一、行向下增长）；
            // 单元格 %<字段>% 先求值为静态快照，居中对齐用 72/73=1/2（与明细表惯例一致）。
            const double rh = std::max(0.5, e.rowHeight);
            const int R = std::max(1, e.tableRows), C = std::max(1, e.tableCols);
            const double th = e.radius > 0.1 ? e.radius : 3.5;
            std::vector<double> xs;
            xs.reserve(size_t(C) + 1);
            xs.push_back(e.pts[0].x);
            for (int c = 0; c < C; ++c) xs.push_back(xs.back() + e.tableColWidth(c));
            // 横线（R+1 条）
            for (int r = 0; r <= R; ++r) {
                const double y = e.pts[0].y - r * rh;
                ent(T, QStringLiteral("LINE"), e);
                T << "10" << num(xs.front()) << "20" << num(y) << "30" << "0.0"
                  << "11" << num(xs.back())  << "21" << num(y) << "31" << "0.0";
            }
            // 竖线（C+1 条）
            for (int c = 0; c <= C; ++c) {
                ent(T, QStringLiteral("LINE"), e);
                T << "10" << num(xs[c]) << "20" << num(e.pts[0].y) << "30" << "0.0"
                  << "11" << num(xs[c]) << "21" << num(e.pts[0].y - R * rh) << "31" << "0.0";
            }
            // 单元格文字（居中：72/73=1/2 + 第二插入点 11/21）
            for (int r = 0; r < R; ++r) {
                for (int c = 0; c < C; ++c) {
                    const QString t = resolveFields(e.tableCell(r, c), nullptr);
                    if (t.trimmed().isEmpty()) continue;
                    const double cx = (xs[c] + xs[c + 1]) / 2.0;
                    const double cy = e.pts[0].y - (r + 0.5) * rh;
                    ent(T, QStringLiteral("TEXT"), e);
                    T << "10" << num(cx) << "20" << num(cy) << "40" << num(th)
                      << "72" << "1" << "73" << "2"
                      << "11" << num(cx) << "21" << num(cy)
                      << "1" << dxfSafe(t);
                }
            }
        } else if (e.type == ElementType::Xline && !e.pts.empty()) {
            // R12 DXF 无 XLINE 原生实体：降级为通过点沿方向 ±10m 的长线段
            const double rad = e.rotation * M_PI / 180.0;
            const Vec3 d(std::cos(rad), std::sin(rad), 0);
            const Vec3 a(e.pts[0].x - d.x * 1e4, e.pts[0].y - d.y * 1e4, 0);
            const Vec3 b(e.pts[0].x + d.x * 1e4, e.pts[0].y + d.y * 1e4, 0);
            ent(T, QStringLiteral("LINE"), e);
            T << "10" << num(a.x) << "20" << num(a.y) << "30" << "0.0"
              << "11" << num(b.x) << "21" << num(b.y) << "31" << "0.0";
        } else if (e.type == ElementType::Point && !e.pts.empty()) {
            // POINT（R12 原生实体）：此前被静默丢弃，现补写（两模式均增强）
            ent(T, QStringLiteral("POINT"), e);
            T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0";
        } else if (e.type == ElementType::Hatch && !e.pts.empty()) {
            if (opts && opt.nativeHatch) {
                // 原生 HATCH：图案线定义（角度/间距随图元）+ 多段线边界路径。
                // 外环 flag=3（external|polyline）、孔 flag=2（polyline）；每顶点 72=0（无凸度）。
                // R12 规范无边界路径组码（91/92+ 为 R13+ 引入），但 ZWCAD/浩辰/GstarCAD 导出
                // R12 均按此事实标准写法、AutoCAD R14+ 读取容错——这是填充在 AutoCAD 中
                // 可编辑的唯一途径。
                ent(T, QStringLiteral("HATCH"), e);
                T << "10" << "0.0" << "20" << "0.0" << "30" << "0.0"
                  << "2" << "ANSI31" << "70" << "0" << "71" << "0"
                  << "78" << "1"
                  << "53" << num(e.rotation)                                 // 图案线角度（度）
                  << "43" << "0.0" << "44" << "0.0"                          // 图案线基点
                  << "45" << "0.0"
                  << "46" << num(e.radius > 0.05 ? e.radius : 2.5)           // 图案线间距
                  << "79" << "0"                                             // 连续线（无 dash）
                  << "91" << QString::number(1 + (int)e.holes.size());       // 边界环数
                const auto writeLoop = [&](const std::vector<Vec3>& loop, const char* flag) {
                    T << "92" << flag << "73" << "1"
                      << "93" << QString::number((int)loop.size());
                    for (const auto& p : loop)
                        T << "72" << "0" << "10" << num(p.x) << "20" << num(p.y);
                };
                writeLoop(e.pts, "3");
                for (const auto& hole : e.holes) writeLoop(hole, "2");
                T << "75" << "0" << "76" << "1";   // style=Normal(奇偶) / predefined 图案
            }
            // nativeHatch 关闭 → 保持旧版行为（静默跳过）
        } else if (e.type == ElementType::BlockRef) {
            // 块引用：优先原生 INSERT（含 ATTRIB 属性值）；外部参照/嵌套过深时兜底
            // expandRef 展开（约束：expandBlockRef 行为仅在块无法原生表达时启用）；
            // 无块表（旧调用方）或定义缺失 → 静默跳过（与旧版一致）。
            const BlockDef* def = nullptr;
            if (blocks)
                for (const auto& d : *blocks)
                    if (d.name == e.blockName && !d.isXref) { def = &d; break; }
            if (def && opts && opt.nativeBlocks && depth < 8) {
                const double sx = std::fabs(e.scaleX) > 1e-12 ? e.scaleX : 1.0;
                const double sy = std::fabs(e.scaleY) > 1e-12 ? e.scaleY : 1.0;
                ent(T, QStringLiteral("INSERT"), e);
                T << "2" << dxfSafe(e.blockName);
                if (!e.blockAttrs.empty()) T << "66" << "1";   // 属性跟随标志
                T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
                  << "41" << num(sx) << "42" << num(sy) << "43" << "1.0"
                  << "50" << num(e.rotation);
                for (const auto& a : e.blockAttrs) {
                    ent(T, QStringLiteral("ATTRIB"), e);
                    T << "10" << num(e.pts[0].x) << "20" << num(e.pts[0].y) << "30" << "0.0"
                      << "40" << "3.5"
                      << "2" << dxfSafe(a.tag)
                      << "1" << dxfSafe(a.value)
                      << "70" << "0";
                }
                if (!e.blockAttrs.empty())
                    T << "0" << "SEQEND" << "8" << dxfLayer(e);
            } else if (def) {
                // 无法原生表达（嵌套超深）：展开为世界坐标子图元递归写出
                const auto subs = block::expandRef(e, *blocks);
                for (const auto& s : subs) writeEntity(s, T, depth + 1);
            }
        }
    };

    for (const auto& e : elems) writeEntity(e, LEnt, 0);
    LEnt << "0" << "ENDSEC";

    // ---- BLOCKS 段：用户块定义（含 ATTDEF 属性定义）+ 原生标注的 *D 匿名块 ----
    L << "0" << "SECTION" << "2" << "BLOCKS";
    if (blocks && opts && opt.nativeBlocks) {
        for (const auto& d : *blocks) {
            if (d.isXref) continue;                              // 外部参照不内联（保持引用语义）
            if (d.name.startsWith(QLatin1Char('*'))) continue;   // 匿名块由本次导出重建
            L << "0" << "BLOCK" << "8" << "0" << "2" << dxfSafe(d.name) << "70" << "0"
              << "10" << num(d.basePoint.x) << "20" << num(d.basePoint.y) << "30" << "0.0";
            for (const auto& ad : d.attdefs)
                L << "0" << "ATTDEF" << "8" << "0"
                  << "10" << num(ad.pos.x) << "20" << num(ad.pos.y) << "30" << "0.0"
                  << "40" << num(ad.height > 0.1 ? ad.height : 3.5)
                  << "1" << dxfSafe(ad.defaultValue)   // 1=默认值
                  << "2" << dxfSafe(ad.tag)            // 2=标签
                  << "3" << dxfSafe(ad.prompt)         // 3=提示语
                  << "70" << QString::number((ad.invisible ? 1 : 0) | (ad.constant ? 2 : 0));
            for (const auto& se : d.entities) writeEntity(se, L, 1);
            L << "0" << "ENDBLK" << "8" << "0";
        }
    }
    for (const auto& kv : anonDims) {
        L << "0" << "BLOCK" << "8" << "0" << "2" << kv.first << "70" << "1"
          << "10" << "0.0" << "20" << "0.0" << "30" << "0.0";
        L << kv.second;
        L << "0" << "ENDBLK" << "8" << "0";
    }
    L << "0" << "ENDSEC";

    // ---- ENTITIES（先行生成的内容原样并入）+ EOF ----
    L << LEnt << "0" << "EOF";

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
            case ElementType::MText: {
                // R12 无 MTEXT：多行 = 首锚点向下按 1.3 倍行距逐行展开为多条 TEXT；
                // %<字段>% 占位先解析为求值快照（与 DXF 导出一致）。
                if(!e.pts.empty() && !e.text.isEmpty()){
                    const double h = e.radius>0.1?e.radius:3.5;
                    const double lineH = h*1.3;
                    QString t = resolveFields(e.text,nullptr);
                    t.replace("\r\n","\n"); t.replace('\r','\n');
                    const QStringList lines = t.split('\n');
                    for(int i=0;i<lines.size();++i){
                        if(lines.at(i).trimmed().isEmpty()) continue;
                        Vec3 p(e.pts[0].x, e.pts[0].y - i*lineH, 0);
                        dwgAddTextR(R, p, h, e.rotation, lines.at(i), layer,color);
                    }
                }
            } break;
            case ElementType::Table: {
                // R12 无 TABLE：炸开为 LINE 网格 + TEXT 单元格文字（几何与画布/DXF 导出同源）。
                if(!e.pts.empty()){
                    const double rh = std::max(0.5, e.rowHeight);
                    const int nR = std::max(1, e.tableRows), nC = std::max(1, e.tableCols);
                    const double h = e.radius>0.1?e.radius:3.5;
                    std::vector<double> xs;
                    xs.reserve(size_t(nC)+1);
                    xs.push_back(e.pts[0].x);
                    for(int c=0;c<nC;++c) xs.push_back(xs.back()+e.tableColWidth(c));
                    for(int r=0;r<=nR;++r)
                        dwgAddLineR(R, Vec3(xs.front(), e.pts[0].y-r*rh,0),
                                       Vec3(xs.back(),  e.pts[0].y-r*rh,0), layer,color);
                    for(int c=0;c<=nC;++c)
                        dwgAddLineR(R, Vec3(xs[c], e.pts[0].y,0),
                                       Vec3(xs[c], e.pts[0].y-nR*rh,0), layer,color);
                    for(int r=0;r<nR;++r)
                        for(int c=0;c<nC;++c){
                            QString t = resolveFields(e.tableCell(r,c), nullptr);
                            if(t.trimmed().isEmpty()) continue;
                            // R12 TEXT 无对齐组码：文字置于单元格左侧 + 1mm 内边距（近似居中观感）
                            Vec3 p(xs[c]+1.0, e.pts[0].y-(r+0.5)*rh, 0);
                            dwgAddTextR(R, p, h, 0.0, t, layer,color);
                        }
                }
            } break;
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
            case ElementType::MText:
            case ElementType::Table:
                if (stats) ++stats->skippedText;           // STEP 曲线集不含文字注记/表格
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

// ---- 线型名 → 本程序 LineStyle（DXF LTYPE 表 / 实体 group 6 通用映射）----
static LineStyle lineStyleFromDxfName(const QString& n) {
    const QString s = n.trimmed().toUpper();
    if (s.isEmpty() || s == "CONTINUOUS" || s == "BYLAYER" || s == "BYBLOCK")
        return LineStyle::Solid;
    if (s.startsWith("DASHED") || s == "HIDDEN") return LineStyle::Dashed;
    if (s == "DOTTED" || s.startsWith("DOT"))    return LineStyle::Dotted;
    if (s.startsWith("CENTER") || s.startsWith("DASHDOT")) return LineStyle::DashDot;
    if (s == "DIVIDE" || s == "PHANTOM" || s.startsWith("DASHDOT2")) return LineStyle::DashDotDot;
    return LineStyle::Solid;
}

// ---- 椭圆弧离散（DXF ELLIPSE 参数化：center + cos(t)·长轴 + sin(t)·短轴）----
// 数学复核：点(t) = c + (a·cos t)·u + (b·sin t)·v，u,v 为长轴/短轴单位向量（v 由 u 旋转 90°）。
static std::vector<Vec3> sampleEllipseArc(const Vec3& c, const Vec3& mj,
                                          double ratio, double s, double e2, int n) {
    const double a = std::hypot(mj.x, mj.y);
    if (a < 1e-12) return {};
    const Vec3 u(mj.x / a, mj.y / a, 0), v(-u.y, u.x, 0);
    const double b = a * ratio;
    double span = e2 - s;
    while (span < 0) span += 2 * M_PI;
    while (span > 2 * M_PI + 1e-9) span -= 2 * M_PI;
    std::vector<Vec3> pts;
    for (int k = 0; k <= n; ++k) {
        const double t = s + span * k / n;
        const double ct = std::cos(t), st = std::sin(t);
        pts.push_back(Vec3(c.x + a * ct * u.x + b * st * v.x,
                           c.y + a * ct * u.y + b * st * v.y, 0));
    }
    return pts;
}

// ---- DXF 回读上下文（ENTITIES 与 BLOCKS 共用一套实体分发）----
// 设计：扫描整份 token 流，按当前 SECTION 把实体分发给 handleEntity；
// 顶层 ENTITIES 的图元进 out，BLOCKS 段内的图元进对应 BlockDef 的 entities；
// 未知/不支持实体只计数跳过，绝不中断整档（约束：未知实体跳过并计数）。
struct DxfImportCtx {
    std::vector<Element>& out;
    DxfImportStats& st;
    int count = 0, arcsKept = 0, arcsDropped = 0;
    std::vector<Vec3> pending;
    std::vector<double> pendingBulge;
    std::vector<DxfPair> pendingProps;
    bool pendingClosed = false, inPoly = false;
    int section = 0;                  // 0 无 / 1 ENTITIES / 2 BLOCKS / 3 TABLES
    int lastBlockRefIdx = -1;         // 最近一个顶层 BlockRef（接收后续 ATTRIB）
    bool inBlock = false;
    BlockDef curBlock;
    std::map<QString, std::vector<Element>> blockEntities;  // name -> 块内图元

    explicit DxfImportCtx(std::vector<Element>& o, DxfImportStats& s) : out(o), st(s) {}

    void applyStyle(Element& e, const std::vector<DxfPair>& p) {
        const QString ly = firstStr(p, 8).trimmed();
        if (!ly.isEmpty()) e.layer = ly;
        if (hasCode(p, 62)) e.color = rgbFromAci((int)firstNum(p, 62, 7));
        const double lw100 = firstNum(p, 370, 13.0);   // 1/100 mm
        e.lineWidth = std::max(1.0, std::round(lw100 / 13.0));
        e.lineWidthAuto = false;
        e.lineStyle = lineStyleFromDxfName(firstStr(p, 6));   // 非 CONTINUOUS → LineStyle 映射
    }

    std::vector<bool> makeArcFlags(const std::vector<Vec3>& pts, const std::vector<double>& bulges,
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
    }

    void flush() {
        if (inPoly && pending.size() >= 2) {
            Element e; e.type = ElementType::Polyline;
            e.pts = pending; e.closed = pendingClosed;
            e.arcFlags = makeArcFlags(pending, pendingBulge, pendingClosed);
            e.name = QStringLiteral("导入:多段线");
            applyStyle(e, pendingProps);
            emitEl(std::move(e));
        }
        pending.clear(); pendingBulge.clear(); pendingProps.clear(); pendingClosed = false; inPoly = false;
    }

    void emitEl(Element e) {
        if (inBlock) curBlock.entities.push_back(std::move(e));
        else { out.push_back(std::move(e)); ++count; }
    }

    void handleEntity(const QString& t, const std::vector<DxfPair>& p);
};

void DxfImportCtx::handleEntity(const QString& t, const std::vector<DxfPair>& p) {
    if (t == "VERTEX") {
        if (inPoly) {
            pending.push_back(Vec3(firstNum(p, 10), firstNum(p, 20), 0));
            pendingBulge.push_back(firstNum(p, 42, 0.0));   // 该顶点出发那一段的凸度
        }
        return;
    }
    if (t == "POLYLINE") { flush(); inPoly = true; pendingClosed = ((int)firstNum(p, 70, 0) & 1) != 0; pendingProps = p; return; }
    if (t == "SEQEND")   { flush(); return; }

    // ---- 原 7 类（保持往返兼容）----
    if (t == "LINE") {
        Element e; e.type = ElementType::Line;
        e.pts = {Vec3(firstNum(p, 10), firstNum(p, 20), 0), Vec3(firstNum(p, 11), firstNum(p, 21), 0)};
        e.name = QStringLiteral("导入:直线"); applyStyle(e, p); emitEl(std::move(e)); return;
    }
    if (t == "CIRCLE") {
        Element e; e.type = ElementType::Circle; e.pts = {Vec3(firstNum(p, 10), firstNum(p, 20), 0)};
        e.radius = firstNum(p, 40); e.name = QStringLiteral("导入:圆"); applyStyle(e, p); emitEl(std::move(e)); return;
    }
    if (t == "ARC") {
        Element e; e.type = ElementType::Arc; e.pts = {Vec3(firstNum(p, 10), firstNum(p, 20), 0)};
        e.radius = firstNum(p, 40); e.startAngle = firstNum(p, 50); e.endAngle = firstNum(p, 51);
        e.name = QStringLiteral("导入:圆弧"); applyStyle(e, p); emitEl(std::move(e)); return;
    }
    if (t == "LWPOLYLINE") {
        std::vector<Vec3> v; std::vector<double> bulges; double lx = 0, ly = 0; bool haveX = false;
        for (const auto& q : p) {
            if (q.code == 10) { lx = q.value.trimmed().toDouble(); haveX = true; }
            else if (q.code == 20) {
                ly = q.value.trimmed().toDouble();
                if (haveX) { v.push_back(Vec3(lx, ly, 0)); bulges.push_back(0.0); haveX = false; }
            } else if (q.code == 42 && !bulges.empty()) bulges.back() = q.value.trimmed().toDouble();
        }
        if (v.size() >= 2) {
            Element e; e.type = ElementType::Polyline; e.pts = v; e.closed = ((int)firstNum(p, 70, 0) & 1) != 0;
            e.arcFlags = makeArcFlags(v, bulges, e.closed); e.name = QStringLiteral("导入:多段线");
            applyStyle(e, p); emitEl(std::move(e));
        }
        return;
    }
    if (t == "TEXT" || t == "MTEXT") {
        const QString s = firstStr(p, 1).trimmed();
        if (!s.isEmpty()) {
            Element e; e.type = ElementType::Text; e.pts = {Vec3(firstNum(p, 10), firstNum(p, 20), 0)};
            e.text = s; e.radius = firstNum(p, 40, 3.5); e.name = QStringLiteral("导入:文本");
            applyStyle(e, p); emitEl(std::move(e));
        }
        return;
    }

    // ---- 扩展实体（任务目标：15+ 类）----

    // ELLIPSE：10/20=圆心，11/21=长轴端点(相对圆心)，40=ratio(短/长)，41/42=参数角起止。
    if (t == "ELLIPSE") {
        Vec3 c(firstNum(p, 10), firstNum(p, 20), 0);
        Vec3 mj(firstNum(p, 11), firstNum(p, 21), 0);
        const double ratio = firstNum(p, 40, 1.0);
        const double a = std::hypot(mj.x, mj.y);
        double start = firstNum(p, 41, 0.0), endp = firstNum(p, 42, 2 * M_PI);
        double span = endp - start;
        while (span < 0) span += 2 * M_PI;
        while (span > 2 * M_PI + 1e-9) span -= 2 * M_PI;
        if (a > 1e-12 && std::fabs(span - 2 * M_PI) < 1e-6) {
            Element e; e.type = ElementType::Ellipse; e.pts = {c};
            e.radius = a; e.radiusY = a * ratio; e.rotation = std::atan2(mj.y, mj.x) * 180.0 / M_PI;
            e.name = QStringLiteral("导入:椭圆"); applyStyle(e, p); emitEl(std::move(e));
        } else {
            std::vector<Vec3> pts = sampleEllipseArc(c, mj, ratio, start, endp, 64);
            if (pts.size() >= 2) {
                Element e; e.type = ElementType::Polyline; e.pts = pts; e.closed = false;
                e.name = QStringLiteral("导入:椭圆弧"); applyStyle(e, p); emitEl(std::move(e));
            } else ++st.fallbackEntities;
        }
        return;
    }

    // SPLINE：控制点(10/20) 或 拟合点(11/21)；优先拟合点重建（Catmull-Rom 插值严格过各点），
    // 无拟合点用控制点；退化(<2点)回落折线。新几何（样条重建）由人工复核。
    if (t == "SPLINE") {
        std::vector<Vec3> ctrl, fit;
        for (size_t k = 0; k + 1 < p.size(); ++k) {
            if (p[k].code == 10 && p[k + 1].code == 20) ctrl.push_back(Vec3(p[k].value.toDouble(), p[k + 1].value.toDouble(), 0));
            if (p[k].code == 11 && p[k + 1].code == 21) fit.push_back(Vec3(p[k].value.toDouble(), p[k + 1].value.toDouble(), 0));
        }
        const std::vector<Vec3>& pts = fit.empty() ? ctrl : fit;
        if (pts.size() >= 2) {
            Element e; e.type = ElementType::Spline; e.pts = pts; e.splineSubType = 0;
            e.name = QStringLiteral("导入:样条"); applyStyle(e, p); emitEl(std::move(e));
        } else ++st.fallbackEntities;
        return;
    }

    // DIMENSION：70=类型。读标注类型与测定点，重建为本产品 Dimension 图元；
    // 复杂(ordinate/未知)回落 LINE+TEXT 并计数。
    if (t == "DIMENSION") {
        const int dt = (int)firstNum(p, 70, 0);
        Vec3 defP(firstNum(p, 10), firstNum(p, 20), 0);   // 定义点（半径/直径时为弧上一点；线性时为备用）
        Vec3 midP(firstNum(p, 11), firstNum(p, 21), 0);   // 标注线中点（文字位置）
        Vec3 p1(firstNum(p, 13), firstNum(p, 23), 0);     // 延伸线原点1（线性/角度）
        Vec3 p2(firstNum(p, 14), firstNum(p, 24), 0);     // 延伸线原点2（线性/角度）
        Vec3 center(firstNum(p, 15), firstNum(p, 25), 0); // 弧圆心(半径/直径) / 角度顶点
        const QString txt = firstStr(p, 1).trimmed();
        Element e; e.type = ElementType::Dimension; e.refA = -1; e.refB = -1; e.text = txt;
        bool ok = false;
        switch (dt) {
            case 0: {   // 旋转/水平/竖直：用 50 角投影量距离，统一存 Distance
                const double ang = firstNum(p, 50, 0.0) * M_PI / 180.0;
                const Vec3 u(std::cos(ang), std::sin(ang), 0);
                const double val = std::fabs((p2.x - p1.x) * u.x + (p2.y - p1.y) * u.y);
                e.dtype = DimensionType::Distance; e.dimValue = val; e.pts = {p1, p2}; ok = true;
            } break;
            case 1: {   // 对齐
                e.dtype = DimensionType::Aligned; e.dimValue = p1.distTo(p2); e.pts = {p1, p2}; ok = true;
            } break;
            case 2: case 5: {   // 角度（顶点=center，两脚=p1/p2）
                const Vec3 a1v(p1.x - center.x, p1.y - center.y, 0), a2v(p2.x - center.x, p2.y - center.y, 0);
                double ang = std::fabs(std::atan2(a1v.y, a1v.x) - std::atan2(a2v.y, a2v.x)) * 180.0 / M_PI;
                while (ang > 180.0001) ang = 360.0 - ang;
                e.dtype = DimensionType::Angle; e.dimValue = ang; e.pts = {center, p1, p2}; ok = true;
            } break;
            case 3: {   // 直径：2 × 圆心到弧上定义点距离
                e.dtype = DimensionType::Diameter; e.dimValue = 2.0 * center.distTo(defP); e.pts = {center, defP}; ok = true;
            } break;
            case 4: {   // 半径：圆心到弧上定义点距离
                e.dtype = DimensionType::Radius; e.dimValue = center.distTo(defP); e.pts = {center, defP}; ok = true;
            } break;
            default: ok = false;   // ordinate/未知 → 降级
        }
        if (ok) { e.name = QStringLiteral("导入:标注"); applyStyle(e, p); emitEl(std::move(e)); }
        else {
            Element ln; ln.type = ElementType::Line; ln.pts = {p1, p2};
            ln.name = QStringLiteral("导入:标注(降级)"); applyStyle(ln, p); emitEl(std::move(ln));
            if (!txt.isEmpty()) {
                Element tx; tx.type = ElementType::Text; tx.pts = {midP}; tx.text = txt;
                tx.radius = firstNum(p, 40, 3.5); tx.name = QStringLiteral("导入:标注文字(降级)");
                applyStyle(tx, p); emitEl(std::move(tx));
            }
            ++st.fallbackEntities;
        }
        return;
    }

    // HATCH：91=边界环数；每环 92 bit2=多段线边界(读 93 顶点数 + 10/20)，否则视为复杂边环。
    // SOLID 与简单多段线边界 → 本产品 Hatch；复杂边环回落边界线并计数；岛检测简化版(holes)。
    if (t == "HATCH") {
        const int nLoops = (int)firstNum(p, 91, 0);
        std::vector<std::vector<Vec3>> loops;
        bool complex = false;
        size_t k = 0;
        for (int loop = 0; loop < nLoops && k < p.size(); ++loop) {
            while (k < p.size() && p[k].code != 92) ++k;
            if (k >= p.size()) break;
            const int flag = (int)p[k].value.toDouble(); ++k;
            if (flag & 2) {   // 多段线边界
                // 规范顺序：92 flag / 73 is_closed(path 级) / 93 顶点数 / [72 has-bulge、
                // 42 bulge、10/20]×n。73/72/42 均可省略（旧样本无这些码）——逐个容错跳过。
                if (k < p.size() && p[k].code == 73) ++k;   // path 级闭合标志（环天然闭合，忽略）
                int vcount = 0;
                if (k < p.size() && p[k].code == 93) { vcount = (int)p[k].value.toDouble(); ++k; }
                std::vector<Vec3> verts; int read = 0;
                while (k < p.size() && read < vcount) {
                    if (p[k].code == 72) { ++k; continue; }   // 顶点级 has-bulge 标志
                    if (p[k].code == 42) { ++k; continue; }   // bulge 值（本产品不用）
                    if (p[k].code == 91) { ++k; continue; }   // source object id（派生路径）
                    if (p[k].code != 10) break;               // 下一环的 92 或其他 → 停
                    const double x = p[k].value.toDouble(); ++k;
                    double y = 0; if (k < p.size() && p[k].code == 20) { y = p[k].value.toDouble(); ++k; }
                    verts.push_back(Vec3(x, y, 0));
                    ++read;
                }
                loops.push_back(verts);
            } else {   // 边环（线/弧/椭圆/样条）→ 复杂
                complex = true;
                while (k < p.size() && p[k].code != 92) ++k;
            }
        }
        if (!complex && !loops.empty() && loops[0].size() >= 3) {
            Element e; e.type = ElementType::Hatch; e.pts = loops[0]; e.closed = true;
            for (size_t L = 1; L < loops.size(); ++L) if (loops[L].size() >= 3) e.holes.push_back(loops[L]);
            e.rotation = 45; e.radius = 2.5; e.lineWidth = 0.5; e.sectionMaterial = 0;
            e.name = QStringLiteral("导入:剖面区域"); applyStyle(e, p); emitEl(std::move(e));
        } else {
            for (auto& lp : loops)
                if (lp.size() >= 2) {
                    Element e; e.type = ElementType::Polyline; e.pts = lp; e.closed = true;
                    e.name = QStringLiteral("导入:填充边界(降级)"); applyStyle(e, p); emitEl(std::move(e));
                }
            ++st.fallbackEntities;
        }
        return;
    }

    // POINT：10/20=位置。
    if (t == "POINT") {
        Element e; e.type = ElementType::Point; e.pts = {Vec3(firstNum(p, 10), firstNum(p, 20), 0)};
        e.name = QStringLiteral("导入:点"); applyStyle(e, p); emitEl(std::move(e)); return;
    }

    // LEADER：顶点在 10/20 序列 → 引线按 Polyline 重建（无专用 LEADER 图元），降级计数。
    if (t == "LEADER") {
        std::vector<Vec3> verts;
        for (size_t k = 0; k + 1 < p.size(); ++k)
            if (p[k].code == 10 && p[k + 1].code == 20) verts.push_back(Vec3(p[k].value.toDouble(), p[k + 1].value.toDouble(), 0));
        if (verts.size() >= 2) {
            Element e; e.type = ElementType::Polyline; e.pts = verts; e.closed = false;
            e.name = QStringLiteral("导入:引线"); applyStyle(e, p); emitEl(std::move(e));
        }
        ++st.fallbackEntities; return;
    }

    // ATTRIB：并入最近一个顶层块引用的属性（tag=2, value=1）。块内 ATTRIB 忽略。
    if (t == "ATTRIB") {
        const QString tag = firstStr(p, 2).trimmed();
        const QString val = firstStr(p, 1).trimmed();
        if (!inBlock && lastBlockRefIdx >= 0 && lastBlockRefIdx < (int)out.size())
            out[lastBlockRefIdx].blockAttrs.push_back({tag, val});
        return;
    }

    // ATTDEF（块定义内的属性声明）：静默忽略，不计数、不中断。
    // 属性"模板"信息对本产品无对应数据（BlockRef 属性值由 ATTRIB 直接携带），跳过即可。
    if (t == "ATTDEF") return;

    // SOLID（实心四边形/三角形）：本产品无填充面图元 → 闭合多段线重建。
    // DXF 顶点序 10/11/12/13 按 1→2→4→3 环绕（3/4 重合即三角形，AutoCAD 标准写法）。
    // 常见于标注匿名块（*D）内的箭头图形，导入后随块过滤；独立 SOLID 亦保留形状。
    if (t == "SOLID") {
        const Vec3 q1(firstNum(p, 10), firstNum(p, 20), 0);
        const Vec3 q2(firstNum(p, 11), firstNum(p, 21), 0);
        const Vec3 q3(firstNum(p, 12), firstNum(p, 22), 0);
        const Vec3 q4(firstNum(p, 13), firstNum(p, 23), 0);
        Element e; e.type = ElementType::Polyline; e.closed = true;
        e.pts = {q1, q2, q4, q3};
        e.name = QStringLiteral("导入:实心面"); applyStyle(e, p); emitEl(std::move(e));
        return;
    }

    // XLINE / RAY：10/20=基点(起点)，11/21=方向向量 → 构造线复用 Xline（pts[0]+rotation）。
    if (t == "XLINE") {
        Vec3 base(firstNum(p, 10), firstNum(p, 20), 0);
        Vec3 dir(firstNum(p, 11), firstNum(p, 21), 0);
        if (dir.x == 0 && dir.y == 0) dir = Vec3(1, 0, 0);
        Element e; e.type = ElementType::Xline; e.pts = {base};
        e.rotation = std::atan2(dir.y, dir.x) * 180.0 / M_PI;
        e.name = QStringLiteral("导入:构造线"); applyStyle(e, p); emitEl(std::move(e)); return;
    }
    if (t == "RAY") {
        Vec3 startP(firstNum(p, 10), firstNum(p, 20), 0);
        Vec3 dir(firstNum(p, 11), firstNum(p, 21), 0);
        if (dir.x == 0 && dir.y == 0) dir = Vec3(1, 0, 0);
        Element e; e.type = ElementType::Xline; e.pts = {startP};
        e.rotation = std::atan2(dir.y, dir.x) * 180.0 / M_PI;
        e.name = QStringLiteral("导入:射线"); applyStyle(e, p); emitEl(std::move(e)); return;
    }

    // INSERT：2=块名，10/20=插入点，41/42=XY缩放，50=旋转，44/45=行列间距，70/71=行列数。
    // 读 BLOCKS 段建块表后，块引用为引用式 BlockRef（仅存名+变换+属性）；MINSERT 阵列逐实例生成。
    if (t == "INSERT") {
        const QString name = firstStr(p, 2).trimmed();
        Vec3 ins(firstNum(p, 10), firstNum(p, 20), 0);
        const double sx = firstNum(p, 41, 1.0), sy = firstNum(p, 42, 1.0);
        const double rot = firstNum(p, 50, 0.0);
        int cols = (int)firstNum(p, 70, 1), rows = (int)firstNum(p, 71, 1);
        const double colSp = firstNum(p, 44, 0), rowSp = firstNum(p, 45, 0);
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        const Vec3 colDir(std::cos(rot * M_PI / 180.0), std::sin(rot * M_PI / 180.0), 0);
        const Vec3 rowDir(-std::sin(rot * M_PI / 180.0), std::cos(rot * M_PI / 180.0), 0);
        lastBlockRefIdx = -1;
        for (int r = 0; r < rows; ++r) for (int c = 0; c < cols; ++c) {
            const Vec3 pos = ins + colDir * (colSp * c) + rowDir * (rowSp * r);
            Element e; e.type = ElementType::BlockRef; e.blockName = name; e.pts = {pos};
            e.scaleX = sx; e.scaleY = sy; e.rotation = rot;
            e.name = QStringLiteral("导入:块引用"); applyStyle(e, p);
            emitEl(std::move(e));
            if (r == 0 && c == 0 && !inBlock) lastBlockRefIdx = (int)out.size() - 1;
        }
        return;
    }

    // 未知/不支持实体：跳过并计数，禁止中断整档（约束）。
    ++st.unknownEntities;
}

bool fromDxf(const QByteArray& data, std::vector<Element>& out, QString* err, DxfImportStats* stats) {
    if (err) err->clear();
    DxfImportStats ls;
    // 先按文件声明的代码页/内容判定解码：我们的写出端中文用 GBK（ANSI_936），
    // 英文/纯 ASCII 用 ANSI_1252，别家 CAD 也可能给 UTF-8。
    const std::vector<DxfPair> tok = tokenizeDxf(decodeDxfBytes(data));
    if (tok.empty()) { if (err) *err = QStringLiteral("文件为空或不是 DXF 文本"); if (stats) *stats = ls; return false; }

    // HEADER $INSUNITS：文件数值的度量口径（AutoCAD 标准码）。缺头/0=无单位按 mm
    // 解释（不缩放）；其余码由调用方经 Units.h::scaleElementsToUnit 整体换算到 mm。
    int insunits = 4;
    for (size_t i = 0; i + 1 < tok.size(); ++i) {
        if (tok[i].code == 9 && tok[i].value.trimmed().compare("$INSUNITS", Qt::CaseInsensitive) == 0) {
            if (tok[i + 1].code == 70) insunits = tok[i + 1].value.trimmed().toInt();
            else for (size_t j = i + 1; j < std::min(tok.size(), i + 4); ++j)
                if (tok[j].code >= 0 && tok[j].value.trimmed().toInt()) { insunits = tok[j].value.trimmed().toInt(); break; }
            break;
        }
    }
    ls.insunits = insunits;

    // 扫描整份 token，按 SECTION 把实体分发给 Ctx.handleEntity（ENTITIES 与 BLOCKS 共用）。
    DxfImportCtx C(out, ls);
    size_t i = 0;
    while (i < tok.size()) {
        if (tok[i].code != 0) { ++i; continue; }
        const QString t = tok[i].value.trimmed().toUpper();
        if (t == "SECTION") {
            if (i + 1 < tok.size() && tok[i + 1].code == 2) {
                const QString sec = tok[i + 1].value.trimmed().toUpper();
                C.section = (sec == "ENTITIES") ? 1 : (sec == "BLOCKS") ? 2 : (sec == "TABLES") ? 3 : 0;
            }
            i += 2; continue;
        }
        if (t == "ENDSEC") { C.flush(); C.section = 0; C.inBlock = false; C.curBlock = BlockDef(); i += 1; continue; }
        if (t == "EOF")    { C.flush(); break; }

        if (C.section == 2 && t == "BLOCK") {
            std::vector<DxfPair> props; size_t j = i + 1; while (j < tok.size() && tok[j].code != 0) { props.push_back(tok[j]); ++j; }
            C.curBlock = BlockDef();
            C.curBlock.name = firstStr(props, 2).trimmed();
            C.curBlock.basePoint = Vec3(firstNum(props, 10), firstNum(props, 20), 0);
            C.inBlock = true;
            i = j; continue;
        }
        if (C.section == 2 && t == "ENDBLK") {
            C.flush();
            if (!C.curBlock.name.isEmpty()) C.blockEntities[C.curBlock.name] = C.curBlock.entities;
            C.curBlock = BlockDef(); C.inBlock = false;
            i += 1; continue;
        }
        if (C.section == 3) {   // TABLES：仅消费（LTYPE 名映射已由 lineStyleFromDxfName 按名处理）
            std::vector<DxfPair> props; size_t j = i + 1; while (j < tok.size() && tok[j].code != 0) { props.push_back(tok[j]); ++j; }
            i = j; continue;
        }
        if (C.section == 1 || C.section == 2) {
            std::vector<DxfPair> props; size_t j = i + 1; while (j < tok.size() && tok[j].code != 0) { props.push_back(tok[j]); ++j; }
            C.handleEntity(t, props);
            i = j; continue;
        }
        ++i;
    }

    // 收尾：BLOCKS 段解析出的块定义交回调用方（MainWindow 负责 addBlockDef）。
    // '*' 前缀为匿名块（*D 标注图形块、*Model_Space / *Paper_Space），不进模型块表：
    // *D 的内容已由 DIMENSION 组码直接重建，进表反而会产生悬空定义。
    for (const auto& kv : C.blockEntities) {
        if (kv.first.startsWith(QLatin1Char('*'))) continue;
        BlockDef d; d.name = kv.first; d.entities = kv.second;
        ls.blocks.push_back(std::move(d));
    }

    if (C.count == 0) {
        if (err) *err = QStringLiteral("ENTITIES 段里没有可识别的图元");
        if (stats) *stats = ls;
        return false;
    }
    ls.entities    = C.count;
    ls.arcsKept    = C.arcsKept;
    ls.arcsDropped = C.arcsDropped;
    if (stats) *stats = ls;
    return true;
}

// ============================================================================
// 外部块表读取（设计中心侧栏）
// ============================================================================
std::vector<BlockDef> blockTableFromJson(const QString& path) {
    std::vector<BlockDef> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QByteArray data = f.readAll();
    f.close();
    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
    if (doc.isNull()) return out;
    // 兼容两种形态：① 顶层对象含 "blocks" 数组（SketchCAD 原生存档）；
    //               ② 顶层即块定义数组（裸块表导出）。
    QJsonArray arr;
    if (doc.isObject() && doc.object().contains(QLatin1String("blocks")))
        arr = doc.object().value(QLatin1String("blocks")).toArray();
    else if (doc.isArray())
        arr = doc.array();
    else
        return out;
    for (const auto& v : arr) {
        if (!v.isObject()) continue;
        BlockDef d = block::blockDefFromJson(v.toObject());
        if (!d.name.trimmed().isEmpty()) out.push_back(std::move(d));
    }
    return out;
}

std::vector<BlockDef> blockTableFromDxf(const QString& path) {
    std::vector<BlockDef> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QByteArray data = f.readAll();
    f.close();
    if (data.isEmpty()) return out;
    std::vector<Element> dummy;
    DxfImportStats st;
    QString err;
    if (fromDxf(data, dummy, &err, &st)) out = std::move(st.blocks);
    return out;
}

} // namespace cadfmt
} // namespace SketchCAD
