#pragma once
#include <QString>
#include <vector>
#include "Element.h"
#include "Geometry.h"   // geo::Seg

// ============================================================================
// GD&T 标注扩展（任务 F）：多重引线（MLeader）+ 形位公差框格（Fcf，GB/T 1182）
// ----------------------------------------------------------------------------
// 与 dim::Style（尺寸标注样式）同一范式：几何生成器放在核心层，国标量都是「图纸毫米」，
// 渲染层只负责把这些线段/文字落到屏幕。故自测可在无头环境直接断言几何产出。
// ============================================================================
namespace SketchCAD {

// ---- 形位公差几何特征符号（GB/T 1182 符号集子集，顺序只增不改）----
// 值为整数并存入 Element::fcfSymbol，故**不得重排**。
enum class FcfSymbol {
    Straightness   = 0,   // 直线度  ─
    Flatness       = 1,   // 平面度  ▱
    Circularity    = 2,   // 圆度    ○
    Cylindricity   = 3,   // 圆柱度  ○夹两平行线
    Parallelism    = 4,   // 平行度  ╱╱
    Perpendicularity= 5,  // 垂直度  ⊥
    Angularity     = 6,   // 倾斜度  ∠
    Position       = 7,   // 位置度  ⊕
    Symmetry       = 8,   // 对称度  三条平行短线
    CircularRunout = 9,   // 圆跳动  ↗ 单箭头
    TotalRunout    = 10   // 全跳动  ⇈ 双箭头
};

int         fcfSymbolCount();
const char* fcfSymbolName(FcfSymbol s);   // 中文名（UI/日志）
bool        fcfSymbolValid(int v);       // 越界防护（Element 存的是 int）

// 一个待绘制的文字格：锚点为文字外框左下基线起点，height 为字高(mm)。
struct GdtText {
    Vec3    anchor;
    double  height = 3.5;
    QString text;
};

// ---- 多重引线几何 ----
struct MLeaderGeom {
    bool valid = false;
    std::vector<geo::Seg> leader;      // 引线折线：附着点 → 折点 → 基线起点
    Vec3   arrowTip;                   // 箭头尖端（实心三角顶点）
    double arrowLen   = 3.5;           // 箭头长(mm)
    double arrowWidth = 1.1;           // 箭头半宽(mm)
    std::vector<geo::Seg> landing;     // 基线（水平段）；为空表示无基线
    std::vector<GdtText> texts;        // 多行文字（按 \n 分行）
};
// 由 Element 生成多重引线几何。dimScale>0 时按绘图比例换算（标注数字同理）。
MLeaderGeom buildMLeader(const Element& e, double dimScale = 1.0);

// ---- 形位公差框格几何 ----
struct FcfCell {
    QRectF   rect;      // 该格矩形（世界坐标，轴对齐；由 rotation 旋转绘制）
    QString  text;      // 格内文字（符号格为空，其笔画在 symbolStrokes）
    bool     isSymbol = false;   // 是否为几何特征符号格
};
struct FcfGeom {
    bool valid = false;
    std::vector<geo::Seg> frame;          // 外框 + 竖分隔线
    std::vector<geo::Seg> symbolStrokes;  // 符号笔画（已离散为线段，含圆）
    std::vector<FcfCell>   cells;         // 各格（含符号格，rect 供拾取/包围盒）
    std::vector<GdtText>   texts;         // 各格文字（锚点在格内居中）
    double height = 7.0;                  // 框格高/字高(mm)
};
// 由 Element 生成形位公差框格几何（各格宽度按内容估算，锚点 pts[0]=框格左下角）。
FcfGeom buildFcf(const Element& e, double dimScale = 1.0);

// ===================== GD&T 标注赋值（标准知识库消费）=====================
// 标注面板（GdtAnnotationDialog）背后的纯逻辑：给定用户输入的数值/特征，从标准表
// （roughness / geo_tol）查得「对应的可选标准列表」，供面板实时刷新、供用户直接选择。
// 全部为无头可测的纯函数，不依赖 GUI。

// 粗糙度标准系列（GB/T 131）：rz=true 取 Rz_series，否则 Ra_series；空表时回落内置序列。
QStringList roughnessSeries(bool rz = false);

// 在粗糙度标准系列中，找与输入数值 v 最接近的标准值（原样保留标准记法，如 "1.6"）。
// v 非法（NaN/负）或无系列 → 返回空 QString。
QString nearestRoughnessValue(double v, bool rz = false);

// 几何公差全部特征（GB/T 1182，geo_tol.features），供面板下拉填充。
QStringList geoTolFeatures();

// 几何公差特征所属类别（形状/方向/位置/跳动），未知特征返回空串。
// 类别键：form / orientation / location / runout（与 geo_tol.json 的 *_features 键对应）。
QString geoTolFeatureCategory(const QString& feature);

// 几何公差可用修饰符（GB/T 1182，geo_tol.modifiers，如 M/L/R/P/F）。
QStringList geoTolModifiers();

// 特征→形位公差框格符号（FcfSymbol 整数）；当前符号集未收录（如 同轴度/线轮廓度/面轮廓度）返回 -1。
int geoTolSymbolForFeature(const QString& feature);

// 组装形位公差框格文本（供预览/日志）：符号名 + 公差值 + 基准 + 修饰符。
QString composeGeoTolText(const QString& feature, const QString& value,
                          const QStringList& datums, const QString& modifier);

} // namespace SketchCAD
