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

} // namespace SketchCAD
