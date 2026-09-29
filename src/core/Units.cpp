#include "Units.h"
#include <QHash>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// ---- JSON 往返（缺键回落默认，旧档案零改动兼容）----
QJsonObject UnitsMeta::toJson() const {
    QJsonObject o;
    o["length"]     = lengthUnitName(length);   // 中文名即持久化键（只增不改）
    o["angle"]      = angleUnitName(angle);
    o["lengthPrec"] = lengthPrec;
    o["anglePrec"]  = anglePrec;
    return o;
}

UnitsMeta UnitsMeta::fromJson(const QJsonObject& o) {
    UnitsMeta u;   // 默认 mm / 十进制度 / 2 位
    if (o.isEmpty()) return u;
    static const QHash<QString, LengthUnit> L = {
        {QStringLiteral("毫米"), LengthUnit::Mm}, {QStringLiteral("厘米"), LengthUnit::Cm},
        {QStringLiteral("米"),   LengthUnit::M},  {QStringLiteral("英寸"), LengthUnit::Inch},
        {QStringLiteral("英尺"), LengthUnit::Foot},
    };
    static const QHash<QString, AngleUnit> A = {
        {QStringLiteral("十进制度"), AngleUnit::Deg}, {QStringLiteral("度分秒"), AngleUnit::DMS},
        {QStringLiteral("弧度"),     AngleUnit::Rad}, {QStringLiteral("百分度"), AngleUnit::Grad},
    };
    const auto lit = L.constFind(o["length"].toString());
    if (lit != L.constEnd()) u.length = lit.value();
    const auto ait = A.constFind(o["angle"].toString());
    if (ait != A.constEnd()) u.angle = ait.value();
    u.lengthPrec = std::clamp(o.contains("lengthPrec") ? o["lengthPrec"].toInt(2) : 2, 0, 6);
    u.anglePrec  = std::clamp(o.contains("anglePrec")  ? o["anglePrec"].toInt(2)  : 2, 0, 6);
    return u;
}

QJsonObject LimitsMeta::toJson() const {
    QJsonObject o;
    o["checkOn"] = checkOn;
    o["x1"] = x1; o["y1"] = y1; o["x2"] = x2; o["y2"] = y2;
    return o;
}

LimitsMeta LimitsMeta::fromJson(const QJsonObject& o) {
    LimitsMeta l;   // 默认 A3 + 不检查
    if (o.isEmpty()) return l;
    l.checkOn = o.contains("checkOn") ? o["checkOn"].toBool(false) : false;
    l.x1 = o.contains("x1") ? o["x1"].toDouble(0.0)   : 0.0;
    l.y1 = o.contains("y1") ? o["y1"].toDouble(0.0)   : 0.0;
    l.x2 = o.contains("x2") ? o["x2"].toDouble(420.0) : 420.0;
    l.y2 = o.contains("y2") ? o["y2"].toDouble(297.0) : 297.0;
    if (!l.isValid()) l = LimitsMeta();
    return l;
}

// ---- 单位换算与展示名 ----
double lengthFactorToMm(LengthUnit u) {
    switch (u) {
        case LengthUnit::Mm:   return 1.0;
        case LengthUnit::Cm:   return 10.0;
        case LengthUnit::M:    return 1000.0;
        case LengthUnit::Inch: return 25.4;
        case LengthUnit::Foot: return 304.8;
    }
    return 1.0;
}

QString lengthUnitName(LengthUnit u) {
    switch (u) {
        case LengthUnit::Mm:   return QStringLiteral("毫米");
        case LengthUnit::Cm:   return QStringLiteral("厘米");
        case LengthUnit::M:    return QStringLiteral("米");
        case LengthUnit::Inch: return QStringLiteral("英寸");
        case LengthUnit::Foot: return QStringLiteral("英尺");
    }
    return QStringLiteral("毫米");
}

QString lengthUnitShort(LengthUnit u) {
    switch (u) {
        case LengthUnit::Mm:   return QStringLiteral("mm");
        case LengthUnit::Cm:   return QStringLiteral("cm");
        case LengthUnit::M:    return QStringLiteral("m");
        case LengthUnit::Inch: return QStringLiteral("in");
        case LengthUnit::Foot: return QStringLiteral("ft");
    }
    return QStringLiteral("mm");
}

QString angleUnitName(AngleUnit u) {
    switch (u) {
        case AngleUnit::Deg:  return QStringLiteral("十进制度");
        case AngleUnit::DMS:  return QStringLiteral("度分秒");
        case AngleUnit::Rad:  return QStringLiteral("弧度");
        case AngleUnit::Grad: return QStringLiteral("百分度");
    }
    return QStringLiteral("十进制度");
}

QString angleUnitShort(AngleUnit u) {
    switch (u) {
        case AngleUnit::Deg:  return QStringLiteral("°");
        case AngleUnit::DMS:  return QStringLiteral("DMS");
        case AngleUnit::Rad:  return QStringLiteral("rad");
        case AngleUnit::Grad: return QStringLiteral("gon");
    }
    return QStringLiteral("°");
}

// DXF $INSUNITS 标准码（AutoCAD 2000+，R12 头部亦普遍兼容）
int insunitsCode(LengthUnit u) {
    switch (u) {
        case LengthUnit::Mm:   return 4;
        case LengthUnit::Cm:   return 5;
        case LengthUnit::M:    return 6;
        case LengthUnit::Inch: return 1;
        case LengthUnit::Foot: return 2;
    }
    return 4;
}

LengthUnit lengthUnitFromInsunits(int code) {
    switch (code) {
        case 1: return LengthUnit::Inch;
        case 2: return LengthUnit::Foot;
        case 4: return LengthUnit::Mm;
        case 5: return LengthUnit::Cm;
        case 6: return LengthUnit::M;
        default: return LengthUnit::Mm;   // 0=无单位/未知码：按 mm 解释并提示
    }
}

// ---- 显示格式化 ----
QString formatLengthMm(double mm, const UnitsMeta& u) {
    const double v = mm / lengthFactorToMm(u.length);
    return QString::number(v, 'f', std::clamp(u.lengthPrec, 0, 6));
}

QString formatLengthWithUnit(double mm, const UnitsMeta& u) {
    return formatLengthMm(mm, u) + QStringLiteral(" ") + lengthUnitShort(u.length);
}

QString formatAngleDeg(double deg, const UnitsMeta& u) {
    switch (u.angle) {
        case AngleUnit::Deg:
            return QString::number(deg, 'f', std::clamp(u.anglePrec, 0, 6));
        case AngleUnit::Rad:
            return QString::number(deg * M_PI / 180.0, 'f', std::clamp(u.anglePrec, 0, 6));
        case AngleUnit::Grad:
            return QString::number(deg / 0.9, 'f', std::clamp(u.anglePrec, 0, 6));
        case AngleUnit::DMS: {
            // 度分秒：precision 作用于秒的小数位
            const bool neg = deg < 0;
            double a = std::fabs(deg);
            int d = int(a);
            a = (a - d) * 60.0;
            int m = int(a);
            double s = (a - m) * 60.0;
            // 进位边界：59.9999…″ 秒 → 60（避免 "60″" 显示）
            if (s >= 59.9995) { s = 0.0; ++m; }
            if (m >= 60)      { m = 0;  ++d; }
            return QString("%1%2%3′%4″")
                    .arg(neg ? QStringLiteral("-") : QString())
                    .arg(d).arg(m, 2, 10, QLatin1Char('0'))
                    .arg(s, 0, 'f', std::clamp(u.anglePrec, 0, 6));
        }
    }
    return QString::number(deg, 'f', 2);
}

// ---- 输入解析（当前单位 → mm；显式后缀覆盖当前单位）----
double parseLengthToMm(const QString& text, const UnitsMeta& u, bool* ok) {
    if (ok) *ok = false;
    QString t = text.trimmed();
    if (t.isEmpty()) return 0.0;

    // 显式单位后缀（不区分大小写、允许中间空格："100 mm" / "2.5m" / 12" / 3'）。
    // **从长到短**排列：mm/cm/in/ft 必须先于 m 匹配，否则 "100mm" 会被 "m" 误吞尾部。
    LengthUnit unit = u.length;
    static const struct { const char* suf; LengthUnit un; } kSuf[] = {
        {"mm", LengthUnit::Mm}, {"cm", LengthUnit::Cm}, {"in", LengthUnit::Inch},
        {"ft", LengthUnit::Foot}, {"m", LengthUnit::M},
        {"\"", LengthUnit::Inch}, {"'", LengthUnit::Foot},
    };
    const QString core = QString(t).toLower().remove(' ');
    for (const auto& s : kSuf) {
        const QString suf = QString::fromLatin1(s.suf);
        if (core.endsWith(suf)) {
            unit = s.un;
            t = t.left(t.size() - suf.size());   // 等长截掉后缀，留下数字主体
            break;
        }
    }
    bool numOk = false;
    const double v = t.trimmed().toDouble(&numOk);
    if (!numOk) return 0.0;
    if (ok) *ok = true;
    return v * lengthFactorToMm(unit);
}

// ---- 跨单位导入换算（原点锚定）----
void scaleElementsToUnit(std::vector<Element>& elems, LengthUnit from) {
    const double f = lengthFactorToMm(from);
    if (f == 1.0 || elems.empty()) return;
    for (auto& e : elems) {
        for (auto& p : e.pts)        { p.x *= f; p.y *= f; }
        for (auto& ring : e.holes)
            for (auto& p : ring)     { p.x *= f; p.y *= f; }
        // 长度量字段：字高/半径/换行宽/表格行高列宽均随度量口径缩放
        e.radius *= f;               // 圆/弧半径、椭圆长轴、文字/单元格字高
        switch (e.type) {
            case ElementType::Ellipse:
                e.radiusY *= f;
                break;
            case ElementType::Table:
                e.rowHeight *= f;
                for (double& w : e.colWidths) w *= f;
                break;
            default:
                break;
        }
        if (e.type == ElementType::Dimension && e.dimTextHeight > 0)
            e.dimTextHeight *= f;    // 标注字高覆盖(mm)
        if (e.mtextWidth > 0) e.mtextWidth *= f;
        // 不动：lineWidth/lineWidthAuto（纸面量）、角度、tolUpper/tolLower（公差按
        // 标注语义随标注文字换算，此处保守不缩，见设计文档说明）
    }
}

} // namespace SketchCAD
