#include "FeatureMenu.h"
#include "SketchModel.h"
#include "Vec.h"
#include <cmath>
#include <algorithm>

namespace SketchCAD {

// ===================== 目录表（单一数据源）=====================
// 顺序 = FeatureId 枚举顺序。分组标题即「扩展工具」菜单下的子菜单标题。
// tip 说明功能与准入条件；禁用时 reason 由 evaluateFeature 单独给出。
const QVector<FeatureEntry>& featureCatalog() {
    static const QVector<FeatureEntry> kCatalog = {
        // ---- P1 中心线家族 ----
        { FeatureId::CenterLine,           "中心线 (P1)", "中心线", "沿选中直线延伸 / 过选中圆心生成单轴中心线（细点画线）。准入：选中含直线或圆" },
        { FeatureId::CenterCross,          "中心线 (P1)", "十字中心线", "过圆心（或直线中点/交点）的一对垂直中心线。准入：选中含圆、直线或点" },
        { FeatureId::CircleMark,           "中心线 (P1)", "圆心标记", "圆中心的小十字标记（多圆批量）。准入：选中含圆" },
        { FeatureId::CornerCenterLine,     "中心线 (P1)", "角点中心线", "过两直线交点的角平分线。准入：选中两条及以上直线" },
        { FeatureId::PlateCenterLine,      "中心线 (P1)", "对称板中心线", "矩形（板）的双对称轴线。准入：选中矩形（圆按整圆十字兜底）" },
        { FeatureId::FullCircleCross,      "中心线 (P1)", "整圆十字中心线", "过整圆的完整十字中心线。准入：选中圆" },
        { FeatureId::AngleCenterLine,      "中心线 (P1)", "角度中心线", "沿角两臂延伸的中心线（批量）。准入：选中含直线" },
        { FeatureId::HoleCenterLine,       "中心线 (P1)", "孔中心线", "过孔中心的单轴中心线。准入：选中圆" },
        { FeatureId::ThroughHoleCenterLine,"中心线 (P1)", "通孔中心线", "过孔中心向两侧长延伸（表示贯通）。准入：选中圆" },
        { FeatureId::TwoLinesCross,        "中心线 (P1)", "两线交点十字线", "过两条已知直线交点的十字中心线。准入：选中两条及以上直线" },
        // ---- P2 序号球标与明细表联动 ----
        { FeatureId::Balloon,              "序号球标 (P2)", "序号球标", "对选中零部件图元编号（圆 + 文字）并登记明细表。准入：选中非空" },
        { FeatureId::BomTable,             "序号球标 (P2)", "明细表...", "查看当前图纸 BOM 明细表数据。准入：图纸已有明细表记录" },
        // ---- P3 符号标注扩展 ----
        { FeatureId::SymbolDatum,          "符号标注 (P3)", "基准符号", "GB 基准代号符号（字母标识）。准入：选中非空（锚点取质心）" },
        { FeatureId::SymbolWeld,           "符号标注 (P3)", "焊接符号", "角焊 / V 形 / 方框焊接符号。准入：选中非空（锚点取质心）" },
        { FeatureId::SymbolTaper,          "符号标注 (P3)", "锥度/斜度符号", "锥度/斜度标注符号（比率文本）。准入：选中非空（锚点取质心）" },
        { FeatureId::SymbolCenterHole,     "符号标注 (P3)", "中心孔符号", "中心孔符号 + 旁注。准入：选中非空（锚点取质心）" },
        { FeatureId::SymbolElevation,      "符号标注 (P3)", "标高符号", "标高符号（±0.000 式值）。准入：选中非空（锚点取质心）" },
        { FeatureId::SymbolRotation,       "符号标注 (P3)", "旋转符号", "旋转符号（角度文本）。准入：选中非空（锚点取质心）" },
        // ---- P4 机械零件设计 ----
        { FeatureId::Gear,                 "机械零件 (P4)", "渐开线齿轮...", "参数化生成齿轮齿廓（齿数/模数/压力角）。准入：无条件（插入视图中心）" },
        // ---- P5 参数化国标图库 ----
        { FeatureId::FastenerBolt,         "国标图库 (P5)", "六角头螺栓...", "GB/T 5782 六角头螺栓出库（公称/杆长）。准入：无条件（插入视图中心）" },
        { FeatureId::FastenerNut,          "国标图库 (P5)", "六角螺母...", "GB/T 6170 六角螺母出库（公称）。准入：无条件（插入视图中心）" },
        { FeatureId::FastenerWasher,       "国标图库 (P5)", "平垫圈...", "GB/T 97.1 平垫圈出库（公称）。准入：无条件（插入视图中心）" },
        // ---- P6 视图工具 ----
        { FeatureId::DirectionSymbol,      "视图工具 (P6)", "向视符号", "向视图符号（箭头 + 字母标识）。准入：选中非空（锚点取质心）" },
        { FeatureId::HoleProjection,       "视图工具 (P6)", "孔轴投影", "孔/轴沿给定方向的投影。准入：选中非空（锚点取质心）" },
        { FeatureId::RotationProjection,   "视图工具 (P6)", "转角投影", "轴线经一次转角后的投影路径。准入：选中非空（锚点取质心）" },
        // ---- P7 辅助工具 ----
        { FeatureId::SectionProps,         "辅助工具 (P7)", "截面特性...", "面积/形心/二次矩/惯性积计算。准入：恰好选中一个闭合轮廓" },
        { FeatureId::MassEstimate,         "辅助工具 (P7)", "重量估算...", "按材料密度估算重量（面积×厚度×密度）。准入：恰好选中一个闭合轮廓" },
        { FeatureId::BatchBom,             "辅助工具 (P7)", "批量 BOM 分析...", "扫描多个图纸文件汇总明细表数量。准入：无条件（文件对话框选源）" },
        // ---- P8 参数化变量层 ----
        { FeatureId::VariableTable,        "参数化变量 (P8)", "变量表...", "定义/驱动/查看参数化变量（表达式求值）。准入：无条件（可新建变量）" },
        // ---- P9 扩展工具 ----
        { FeatureId::SplitDrawing,         "扩展工具 (P9)", "机械图拆分...", "按图层或图框把图元分组写出子档。准入：图纸含图元" },
        { FeatureId::PackDrawings,         "扩展工具 (P9)", "自动排图...", "shelf 装箱计算图框落位（报告不改模型）。准入：图纸含图框（图框层矩形）" },
        { FeatureId::BatchSign,            "扩展工具 (P9)", "批量签名...", "按字段表批量填写标题栏。准入：图纸含标题栏字段(tb:field:)" },
    };
    return kCatalog;
}

const FeatureEntry* featureEntry(FeatureId id) {
    const int idx = (int)id;
    const QVector<FeatureEntry>& cat = featureCatalog();
    if (idx < 0 || idx >= cat.size() || cat[idx].id != id) return nullptr;
    return &cat[idx];
}

// ===================== 内部谓词 =====================
namespace {

FeatureAvailability ok() { return FeatureAvailability{}; }
FeatureAvailability no(const QString& reason) {
    FeatureAvailability a;
    a.enabled = false;
    a.reason  = reason;
    return a;
}

// 选择集中是否有任一指定类型的图元（按 id 反查，缺失 id 跳过）。
bool selectionHasType(const SketchModel& m, std::initializer_list<ElementType> types) {
    for (int id : m.selection()) {
        const Element* e = m.element(id);
        if (!e) continue;
        for (ElementType t : types)
            if (e->type == t) return true;
    }
    return false;
}

int selectionTypeCount(const SketchModel& m, ElementType t) {
    int n = 0;
    for (int id : m.selection()) {
        const Element* e = m.element(id);
        if (e && e->type == t) ++n;
    }
    return n;
}

// 标题栏字段元素（name 以 "tb:field:" 开头——与 sheet_frame 生成端同源约定）。
// ⚠️ 中文/前缀比较禁用 QLatin1String(utf8)：一律 QStringLiteral（UTF-16 字面量）。
bool hasTitleBlockField(const SketchModel& m) {
    static const QString kPrefix = QStringLiteral("tb:field:");
    for (const Element& e : m.elements())
        if (e.name.startsWith(kPrefix)) return true;
    return false;
}

// 图框层矩形（拆分「按图框」/ 排图的输入；层名与图框生成端同源，见 sheet_frame）。
bool hasFrameRect(const SketchModel& m) {
    for (const Element& e : m.elements())
        if (e.type == ElementType::Rectangle && e.layer == QStringLiteral("图框"))
            return true;
    return false;
}

// 选择集非空（锚点/refs 来源）。
FeatureAvailability needSelection(const QString& what) {
    return no(QStringLiteral("请先选中%1（当前选择集为空）").arg(what));
}

} // namespace

// ===================== 准入评估 =====================
FeatureAvailability evaluateFeature(const SketchModel& model, FeatureId id) {
    const bool hasSel = model.hasSelection();

    switch (id) {
    // ---- P1 中心线家族：条件与 CenterLineGen 各 kind 的 refs 语义严格一致 ----
    case FeatureId::CenterLine:
        if (!hasSel || !(selectionHasType(model, { ElementType::Line, ElementType::Circle })))
            return no(QStringLiteral("请先选中直线或圆（中心线沿直线延伸或过圆心生成）"));
        return ok();
    case FeatureId::CenterCross:
        if (!hasSel || !(selectionHasType(model, { ElementType::Circle, ElementType::Line, ElementType::Point })))
            return no(QStringLiteral("请先选中圆、直线或点（十字中心线过其中心/中点/交点）"));
        return ok();
    case FeatureId::CircleMark:
        if (!hasSel || !selectionHasType(model, { ElementType::Circle }))
            return no(QStringLiteral("请先选中圆（圆心标记以有效圆为参考）"));
        return ok();
    case FeatureId::CornerCenterLine:
        if (selectionTypeCount(model, ElementType::Line) < 2)
            return no(QStringLiteral("请先选中两条直线（角点中心线过其交点）"));
        return ok();
    case FeatureId::PlateCenterLine:
        if (!hasSel || !(selectionHasType(model, { ElementType::Rectangle, ElementType::Circle })))
            return no(QStringLiteral("请先选中矩形（对称板；圆按整圆十字兜底）"));
        return ok();
    case FeatureId::FullCircleCross:
        if (!hasSel || !selectionHasType(model, { ElementType::Circle }))
            return no(QStringLiteral("请先选中整圆"));
        return ok();
    case FeatureId::AngleCenterLine:
        if (!hasSel || !selectionHasType(model, { ElementType::Line }))
            return no(QStringLiteral("请先选中直线（角度中心线沿角两臂延伸）"));
        return ok();
    case FeatureId::HoleCenterLine:
        if (!hasSel || !selectionHasType(model, { ElementType::Circle }))
            return no(QStringLiteral("请先选中圆（孔）"));
        return ok();
    case FeatureId::ThroughHoleCenterLine:
        if (!hasSel || !selectionHasType(model, { ElementType::Circle }))
            return no(QStringLiteral("请先选中圆（通孔）"));
        return ok();
    case FeatureId::TwoLinesCross:
        if (selectionTypeCount(model, ElementType::Line) < 2)
            return no(QStringLiteral("请先选中两条直线（交点十字线）"));
        return ok();

    // ---- P2 序号球标与明细表 ----
    case FeatureId::Balloon:
        if (!hasSel) return needSelection(QStringLiteral("要编号的零部件图元"));
        return ok();
    case FeatureId::BomTable:
        if (model.bomCount() <= 0)
            return no(QStringLiteral("图纸尚无明细表数据（请先用「序号球标」编号生成）"));
        return ok();

    // ---- P3 符号标注：锚点取选中图元质心 ----
    case FeatureId::SymbolDatum:
    case FeatureId::SymbolWeld:
    case FeatureId::SymbolTaper:
    case FeatureId::SymbolCenterHole:
    case FeatureId::SymbolElevation:
    case FeatureId::SymbolRotation:
        if (!hasSel) return needSelection(QStringLiteral("图元以确定符号放置位置（锚点取其质心）"));
        return ok();

    // ---- P4 / P5：参数化对话框 + 插入视图中心，无需预选 ----
    case FeatureId::Gear:
    case FeatureId::FastenerBolt:
    case FeatureId::FastenerNut:
    case FeatureId::FastenerWasher:
        return ok();

    // ---- P6 视图工具：锚点取选中图元质心 ----
    case FeatureId::DirectionSymbol:
    case FeatureId::HoleProjection:
    case FeatureId::RotationProjection:
        if (!hasSel) return needSelection(QStringLiteral("图元以确定符号锚点（取其质心）"));
        return ok();

    // ---- P7 辅助工具 ----
    case FeatureId::SectionProps:
    case FeatureId::MassEstimate:
        if (model.selection().size() != 1)
            return no(QStringLiteral("请恰好选中一个闭合轮廓（多段线/矩形/圆/边界）"));
        {
            const Element* e = model.element(model.selection().front());
            if (!e || !isClosedProfile(*e))
                return no(QStringLiteral("选中图元不是闭合轮廓（多段线/矩形/圆/边界）"));
        }
        return ok();
    case FeatureId::BatchBom:
        return ok();   // 扫描外部文件，与当前图纸无关

    // ---- P8 变量表：会话级，可新建 ----
    case FeatureId::VariableTable:
        return ok();

    // ---- P9 扩展工具 ----
    case FeatureId::SplitDrawing:
        if (model.elementCount() <= 0)
            return no(QStringLiteral("图纸为空，无可拆分内容"));
        return ok();
    case FeatureId::PackDrawings:
        if (!hasFrameRect(model))
            return no(QStringLiteral("图纸中未找到图框（图框层矩形），无可排布内容"));
        return ok();
    case FeatureId::BatchSign:
        if (!hasTitleBlockField(model))
            return no(QStringLiteral("图纸中未找到标题栏字段（请先「按幅面生成图框与标题栏」）"));
        return ok();

    case FeatureId::FeatureIdCount:
        break;
    }
    // 未知 id：保守禁用（防新增枚举漏配评估规则时被误放行）
    return no(QStringLiteral("未知功能入口（未配置准入规则）"));
}

FeatureAvailability evaluateFeature(const SketchModel* model, FeatureId id) {
    if (!model) {
        // 空图纸：仅无条件类功能可用（齿轮/紧固件/批量BOM/变量表）
        switch (id) {
        case FeatureId::Gear:
        case FeatureId::FastenerBolt:
        case FeatureId::FastenerNut:
        case FeatureId::FastenerWasher:
        case FeatureId::BatchBom:
        case FeatureId::VariableTable:
            return FeatureAvailability{};
        default:
            break;
        }
        return no(QStringLiteral("无可用图纸"));
    }
    return evaluateFeature(*model, id);
}

// ===================== P1 映射 =====================
bool isCenterLineFeature(FeatureId id) {
    switch (id) {
    case FeatureId::CenterLine:          return true;
    case FeatureId::CenterCross:         return true;
    case FeatureId::CircleMark:          return true;
    case FeatureId::CornerCenterLine:    return true;
    case FeatureId::PlateCenterLine:     return true;
    case FeatureId::FullCircleCross:     return true;
    case FeatureId::AngleCenterLine:     return true;
    case FeatureId::HoleCenterLine:      return true;
    case FeatureId::ThroughHoleCenterLine: return true;
    case FeatureId::TwoLinesCross:       return true;
    default:                             return false;
    }
}

CenterLineKind centerLineKindFor(FeatureId id) {
    switch (id) {
    case FeatureId::CenterLine:            return CenterLineKind::kCenter;
    case FeatureId::CenterCross:           return CenterLineKind::kCross;
    case FeatureId::CircleMark:            return CenterLineKind::kCircleMark;
    case FeatureId::CornerCenterLine:      return CenterLineKind::kCorner;
    case FeatureId::PlateCenterLine:       return CenterLineKind::kPlate;
    case FeatureId::FullCircleCross:       return CenterLineKind::kFullCircle;
    case FeatureId::AngleCenterLine:       return CenterLineKind::kAngle;
    case FeatureId::HoleCenterLine:        return CenterLineKind::kHole;
    case FeatureId::ThroughHoleCenterLine: return CenterLineKind::kThroughHole;
    case FeatureId::TwoLinesCross:         return CenterLineKind::kTwoLinesCross;
    default:                               return CenterLineKind::kCenter;
    }
}

// ===================== 闭合轮廓 =====================
bool isClosedProfile(const Element& e) {
    switch (e.type) {
    case ElementType::Circle:
        return e.radius > 0.0;
    case ElementType::Rectangle:
    case ElementType::Polygon:
    case ElementType::Boundary:
        return true;                       // 矩形/多边形天然闭合；边界定义即闭合环
    case ElementType::Polyline:
        return e.closed && e.pts.size() >= 3;
    default:
        return false;
    }
}

bool closedProfilePolygon(const Element& e, std::vector<Vec3>& out) {
    out.clear();
    if (!isClosedProfile(e)) return false;

    switch (e.type) {
    case ElementType::Circle: {
        // 128 边形近似（与 t_sectionprops 圆对照解析解同源）
        const int n = 128;
        const Vec3 c = e.pts.empty() ? Vec3() : e.pts[0];
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            const double a = 2.0 * M_PI * i / n;
            out.push_back(Vec3(c.x + e.radius * std::cos(a), c.y + e.radius * std::sin(a), 0.0));
        }
        return true;
    }
    case ElementType::Rectangle: {
        if (e.pts.size() < 2) return false;
        const Vec3& a = e.pts[0];
        const Vec3& b = e.pts[1];
        out = { Vec3(a.x, a.y, 0), Vec3(b.x, a.y, 0), Vec3(b.x, b.y, 0), Vec3(a.x, b.y, 0) };
        return true;
    }
    case ElementType::Polygon:
    case ElementType::Polyline:
    case ElementType::Boundary: {
        if (e.pts.size() < 3) return false;
        out = e.pts;
        return true;
    }
    default:
        return false;
    }
}

} // namespace SketchCAD
