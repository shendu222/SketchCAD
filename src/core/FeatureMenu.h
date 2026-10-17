#pragma once
// 功能菜单与准入（P1~P9 菜单集成层）
// ------------------------------------------------------------
// 背景（用户需求）：为 P1~P9 各项功能在 UI 中添加菜单入口，并实现菜单
// 准入条件判断——满足预设条件的菜单项可点击进入，不满足则置灰禁用
// （QAction::setEnabled(false)），且禁用原因可读（供 statusTip / 悬停反馈）。
//
// 设计要点：
//   * 本模块为**纯逻辑**（只依赖 SketchModel 只读查询），不依赖任何 Qt UI
//     类——准入规则可无头自测（t_featuremenu），菜单壳（MainWindow）只负责
//     注册 QAction + aboutToShow 时调用 evaluateFeature + 触发分发。
//   * 单一数据源：featureCatalog() 是菜单结构的唯一事实源（分组/标题/提示），
//     MainWindow 的「扩展工具」菜单与命令面板/帮助文本均应由此生成，禁止两处
//     手工维护（同一语义两处手工拼装 ⇒ 一处更新一处漏，工程既有血泪）。
//   * 准入条件与库函数真实语义严格一致（如 kCircleMark 需选中圆、
//     kCorner 需两条直线、截面特性需单选闭合轮廓）——评估放行的操作，
//     库层不得再因同类原因失败；库层仍保留自身退化防御（双保险）。
//   * reason 文案即「交互反馈」：禁用态下菜单项 toolTip/statusTip 展示原因，
//     用户悬停即可得知「缺什么」而非盲目点击无效。
#include <QString>
#include <QVector>
#include <vector>
#include "CenterLineGen.h"   // P1：CenterLineKind（菜单项 ↔ 类型映射）
#include "Element.h"         // ElementType / Element（准入谓词）

namespace SketchCAD {

class SketchModel;

// ===================== 功能菜单条目 id =====================
// P1~P9 全部可菜单化入口（32 项）。枚举「只增不改」——值序即目录顺序，
// 自测按值断言；新增功能必须追加到末尾（FeatureIdCount 之前）。
enum class FeatureId {
    // ---- P1 中心线家族（10 种）----
    CenterLine = 0,          // 中心线（沿轴/直线延伸）
    CenterCross,             // 十字中心线（过圆心/中点/交点）
    CircleMark,              // 圆心标记（圆/圆弧中心小十字）
    CornerCenterLine,        // 角点中心线（两直线交点角平分线）
    PlateCenterLine,         // 对称板中心线（矩形双对称轴）
    FullCircleCross,         // 整圆十字中心线
    AngleCenterLine,         // 角度中心线（沿角两臂延伸）
    HoleCenterLine,          // 孔中心线（过孔中心单轴）
    ThroughHoleCenterLine,   // 通孔中心线（两侧长延伸）
    TwoLinesCross,           // 两线交点十字线
    // ---- P2 序号球标与明细表联动 ----
    Balloon,                 // 序号球标（对选中图元编号）
    BomTable,                // 明细表（BOM 数据查看）
    // ---- P3 符号标注扩展（SymbolFactory 六种）----
    SymbolDatum,             // 基准符号
    SymbolWeld,              // 焊接符号
    SymbolTaper,             // 锥度/斜度符号
    SymbolCenterHole,        // 中心孔符号
    SymbolElevation,         // 标高符号
    SymbolRotation,          // 旋转符号
    // ---- P4 机械零件设计 ----
    Gear,                    // 渐开线齿轮
    // ---- P5 参数化国标图库 ----
    FastenerBolt,            // 六角头螺栓
    FastenerNut,             // 六角螺母
    FastenerWasher,          // 平垫圈
    // ---- P6 视图工具 ----
    DirectionSymbol,         // 向视符号
    HoleProjection,          // 孔轴投影
    RotationProjection,      // 转角投影
    // ---- P7 辅助工具 ----
    SectionProps,            // 截面特性（面积/形心/二次矩/惯性积）
    MassEstimate,            // 重量估算
    BatchBom,                // 批量 BOM 分析
    // ---- P8 参数化变量层 ----
    VariableTable,           // 变量表（定义/驱动/查看）
    // ---- P9 扩展工具 ----
    SplitDrawing,            // 机械图拆分（按图层/图框）
    PackDrawings,            // 自动排图（shelf 装箱）
    BatchSign,               // 批量签名（标题栏字段）

    FeatureIdCount           // 哨兵：条目总数（必须最后）
};

// ===================== 准入评估结果 =====================
struct FeatureAvailability {
    bool    enabled = true;   // true = 可点击进入；false = 置灰禁用
    QString reason;           // 禁用原因（enabled 时为空串）——交互反馈文案
};

// ===================== 目录条目 =====================
struct FeatureEntry {
    FeatureId   id;
    const char* group;   // 菜单分组标题（P1~P9 语义组，即子菜单标题）
    const char* title;   // 菜单项标题（源语言中文；UI 可再包 tr()）
    const char* tip;     // 提示文本（功能说明 + 准入条件），statusTip 用
};

// 全量目录（菜单注册唯一数据源；顺序 = 枚举顺序）。
const QVector<FeatureEntry>& featureCatalog();

// 按 id 查目录条目；越界 → nullptr。
const FeatureEntry* featureEntry(FeatureId id);

// ===================== 准入条件评估 =====================
// 对当前图纸/选择集评估某功能是否可进入。model 为空指针 → 视为空图纸
// （仅「无条件类」功能启用）。纯只读，可无头直测。
FeatureAvailability evaluateFeature(const SketchModel& model, FeatureId id);
FeatureAvailability evaluateFeature(const SketchModel* model, FeatureId id);

// ===================== 共享几何工具 =====================
// P1 菜单项 → 中心线类型（非 P1 条目 → kCenter；isCenterLineFeature 可先行判定）。
bool             isCenterLineFeature(FeatureId id);
CenterLineKind   centerLineKindFor(FeatureId id);

// 提取「闭合轮廓」多边形（供 P7 截面特性/重量估算）：
//   Circle → 128 边形近似（与 t_sectionprops 圆对照同源）；
//   Rectangle → 四角（pts 两个对角点展开）；
//   Polygon / Polyline(closed) / Boundary → 控制点序列。
// 非闭合轮廓 / 顶点不足 → false（out 清空）。
// 与准入谓词 isClosedProfile 同源：评估放行 ⇔ 本函数可提取。
bool closedProfilePolygon(const Element& e, std::vector<Vec3>& out);

// 图元是否为闭合轮廓（P7 准入谓词）。
bool isClosedProfile(const Element& e);

} // namespace SketchCAD
