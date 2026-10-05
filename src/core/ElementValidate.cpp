#include "ElementValidate.h"

#include <cmath>
#include <unordered_set>
#include <QStringList>

namespace SketchCAD {

namespace {

constexpr double kMaxCoord   = 1e18;   // 坐标/半径上限：超出视为损坏（渲染/包围盒会溢出）
constexpr double kEps        = 1e-9;   // 退化判定容差
constexpr int    kMaxTextLen = 1 << 20; // 文本字段上限 1MB：超出视为损坏（防内存耗尽）
constexpr double kMaxScale   = 1e6;    // 块插入缩放上限：超出视为损坏

inline bool coordBad(double v) {
    return !std::isfinite(v) || std::fabs(v) > kMaxCoord;
}

inline bool pointBad(const Vec3& p) {
    return coordBad(p.x) || coordBad(p.y) || coordBad(p.z);
}

// 文本字段里的控制字符（保留 \t \n \r，因其用于多行文本排版）。
inline bool isControlChar(ushort u) {
    return (u < 0x09)                       // 0x00..0x08
        || (u > 0x0A && u < 0x0D)           // 0x0B..0x0C
        || (u > 0x0D && u < 0x20);          // 0x0E..0x1F
}

bool hasControlChar(const QString& s) {
    for (QChar c : s)
        if (isControlChar(c.unicode())) return true;
    return false;
}

QString stripControlChars(const QString& s) {
    QString out;
    out.reserve(s.size());
    for (QChar c : s)
        if (!isControlChar(c.unicode())) out.append(c);
    return out;
}

// 哪些类型必须有控制点几何。
bool needsPoints(ElementType t) {
    switch (t) {
        case ElementType::Line:
        case ElementType::Polyline:
        case ElementType::Spline:
        case ElementType::Rectangle:
        case ElementType::Polygon:
        case ElementType::Boundary:
        case ElementType::Hatch:
        case ElementType::MLeader:
        case ElementType::Xline:
            return true;
        default:
            return false; // Text/MText/Table/BlockRef/Point/Fcf/Constraint/Dimension 几何由其它方式表达或可选
    }
}

// 该类图元最少控制点数（不足即退化）。
int minPoints(ElementType t) {
    switch (t) {
        case ElementType::Polygon:
        case ElementType::Boundary: return 3;
        case ElementType::Hatch:    return 1; // 至少一个外边界点；空 pts 但有 holes 也允许（下方单独判）
        default:                    return 2;
    }
}

bool isClosedLoopType(ElementType t) {
    return t == ElementType::Polyline
        || t == ElementType::Polygon
        || t == ElementType::Rectangle
        || t == ElementType::Boundary;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 单图元只读校验
// ---------------------------------------------------------------------------
bool isElementValid(const Element& e, QString* why) {
    const int ti = static_cast<int>(e.type);
    if (ti < 0 || ti > static_cast<int>(ElementType::Fcf)) {
        if (why) *why = QStringLiteral("图元类型越界(%1)").arg(ti);
        return false;
    }
    // 文本 / 名称类字段：超长（防内存耗尽）
    if (e.text.size() > kMaxTextLen) {
        if (why) *why = QStringLiteral("文本超长(%1 字节)").arg(e.text.size());
        return false;
    }
    if (e.name.size() > kMaxTextLen) {
        if (why) *why = QStringLiteral("名称超长");
        return false;
    }
    // 数值字段有限性 / 范围
    if (coordBad(e.radius) || coordBad(e.radiusY) || coordBad(e.rotation)
        || coordBad(e.startAngle) || coordBad(e.endAngle)
        || coordBad(e.tolUpper) || coordBad(e.tolLower)
        || coordBad(e.dimValue) || coordBad(e.lineWidth)
        || coordBad(e.widthFactor) || coordBad(e.lineSpacing) || coordBad(e.mtextWidth)
        || coordBad(e.scaleX) || coordBad(e.scaleY)
        || coordBad(e.rowHeight) || coordBad(e.tableRows) || coordBad(e.tableCols)) {
        if (why) *why = QStringLiteral("数值字段含 NaN/Inf/越界");
        return false;
    }
    // 块插入缩放越界
    if (std::fabs(e.scaleX) > kMaxScale || std::fabs(e.scaleY) > kMaxScale) {
        if (why) *why = QStringLiteral("块插入缩放越界");
        return false;
    }
    // 控制点坐标
    for (const auto& p : e.pts) {
        if (pointBad(p)) {
            if (why) *why = QStringLiteral("控制点坐标含 NaN/Inf/越界");
            return false;
        }
    }
    // 内孔坐标
    for (const auto& ring : e.holes) {
        for (const auto& p : ring) {
            if (pointBad(p)) {
                if (why) *why = QStringLiteral("内孔坐标含 NaN/Inf/越界");
                return false;
            }
        }
    }
    // 类型相关退化判定
    if (needsPoints(e.type)) {
        const int mp = minPoints(e.type);
        if ((int)e.pts.size() < mp) {
            if (why) *why = QStringLiteral("控制点不足(需要%1，实际%2)").arg(mp).arg(e.pts.size());
            return false;
        }
        if (e.type == ElementType::Hatch && e.pts.empty() && e.holes.empty()) {
            if (why) *why = QStringLiteral("剖面区域无边界");
            return false;
        }
    }
    // 直线零长度
    if (e.type == ElementType::Line && e.pts.size() >= 2 && e.pts[0] == e.pts[1]) {
        if (why) *why = QStringLiteral("直线零长度");
        return false;
    }
    // 圆 / 椭圆 / 圆弧 零半径
    if ((e.type == ElementType::Circle || e.type == ElementType::Arc) && e.radius <= kEps) {
        if (why) *why = QStringLiteral("圆/圆弧零半径");
        return false;
    }
    if (e.type == ElementType::Ellipse && (e.radius <= kEps || e.radiusY <= kEps)) {
        if (why) *why = QStringLiteral("椭圆零半轴");
        return false;
    }
    // 圆弧零扫掠角
    if (e.type == ElementType::Arc) {
        const double sweep = std::fabs(e.endAngle - e.startAngle);
        if (sweep < kEps || std::fabs(sweep - 360.0) < kEps) {
            if (why) *why = QStringLiteral("圆弧零扫掠角");
            return false;
        }
    }
    // 闭合折线首尾重复点（可修复，由 filterElements 走修复分支）
    if (isClosedLoopType(e.type) && e.closed && e.pts.size() >= 2
        && e.pts.front() == e.pts.back()) {
        if (why) *why = QStringLiteral("闭合折线首尾重复点(可修复)");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// 批量守卫
// ---------------------------------------------------------------------------
std::vector<Element> filterElements(const std::vector<Element>& in,
                                   int* skipCount, int* fixCount, QString* report) {
    std::vector<Element> out;
    out.reserve(in.size());
    int skip = 0, fix = 0;
    QStringList lines;

    for (const auto& e : in) {
        const int ti = static_cast<int>(e.type);
        // 1) 类型越界：直接丢弃（无法判定几何）
        if (ti < 0 || ti > static_cast<int>(ElementType::Fcf)) {
            ++skip;
            lines << QStringLiteral("跳过类型越界图元(type=%1)").arg(ti);
            continue;
        }
        // 2) 构建候选：先尝试裁剪修复（去重复末点 / 剥离控制字符）
        Element cand = e;
        bool repaired = false;
        if (isClosedLoopType(e.type) && e.closed && e.pts.size() >= 2
            && e.pts.front() == e.pts.back()) {
            cand.pts.pop_back();      // 去掉重复末点；闭合由 closed 标志表达
            repaired = true;
        }
        if (hasControlChar(e.text) || hasControlChar(e.name)
            || hasControlChar(e.styleName) || hasControlChar(e.layer)) {
            cand.text      = stripControlChars(e.text);
            cand.name      = stripControlChars(e.name);
            cand.styleName = stripControlChars(e.styleName);
            cand.layer     = stripControlChars(e.layer);
            if (cand.layer.isEmpty()) cand.layer = QStringLiteral("0"); // 图层落空回落保留层
            repaired = true;
        }
        // 3) 校验候选
        QString why;
        if (isElementValid(cand, &why)) {
            out.push_back(cand);
            if (repaired) {
                ++fix;
                lines << QStringLiteral("修复图元(id=%1): %2").arg(e.id)
                         .arg(repaired ? QStringLiteral("裁剪重复末点/剥离控制字符") : QString());
            }
        } else {
            ++skip;
            lines << QStringLiteral("跳过损坏图元(id=%1, type=%2): %3").arg(e.id).arg(ti).arg(why);
        }
    }

    if (skipCount) *skipCount = skip;
    if (fixCount)  *fixCount  = fix;
    if (report)    *report    = lines.join(QStringLiteral("；"));
    return out;
}

// ---------------------------------------------------------------------------
// 缺失引用统计（打开日志如实告知，不丢弃）
// ---------------------------------------------------------------------------
int countDanglingRefs(const std::vector<Element>& elems,
                     const std::vector<QString>& layerNames,
                     const std::vector<QString>& blockNames,
                     const std::vector<QString>& styleNames,
                     QString* report) {
    std::unordered_set<int> ids;
    ids.reserve(elems.size());
    for (const auto& e : elems) ids.insert(e.id);

    const std::unordered_set<QString> lay(layerNames.begin(), layerNames.end());
    const std::unordered_set<QString> blk(blockNames.begin(), blockNames.end());
    const std::unordered_set<QString> sty(styleNames.begin(), styleNames.end());

    int missingLayer = 0, missingBlock = 0, missingStyle = 0, danglingRef = 0;

    for (const auto& e : elems) {
        if (!lay.count(e.layer)) ++missingLayer;
        if (e.type == ElementType::BlockRef && !e.blockName.isEmpty()
            && !blk.count(e.blockName)) ++missingBlock;
        if ((e.type == ElementType::Text || e.type == ElementType::MText)
            && !e.styleName.isEmpty() && !sty.count(e.styleName)) ++missingStyle;
        if (e.type == ElementType::Constraint || e.type == ElementType::Dimension) {
            if (e.refA >= 0 && !ids.count(e.refA)) ++danglingRef;
            if (e.refB >= 0 && !ids.count(e.refB)) ++danglingRef;
        }
        if (e.type == ElementType::Hatch) {
            for (int sid : e.hatchSrcIds)
                if (!ids.count(sid)) ++danglingRef;
        }
    }

    const int total = missingLayer + missingBlock + missingStyle + danglingRef;
    if (report) {
        QStringList parts;
        if (missingLayer)  parts << QStringLiteral("缺失图层引用 %1").arg(missingLayer);
        if (missingBlock)  parts << QStringLiteral("缺失块引用 %1").arg(missingBlock);
        if (missingStyle)  parts << QStringLiteral("缺失文字样式引用 %1").arg(missingStyle);
        if (danglingRef)   parts << QStringLiteral("悬空几何引用(refA/refB/hatchSrcIds) %1").arg(danglingRef);
        *report = parts.join(QStringLiteral("；"));
    }
    return total;
}

} // namespace SketchCAD
