#pragma once
#include <QString>
#include <vector>
#include "core/Element.h"
#include "core/Vec.h"

// ============================================================================
// 焊缝符号（GB/T 324-2008 焊缝符号表示法 / GB/T 12212 焊缝符号的尺寸、比例及简法）
// ----------------------------------------------------------------------------
// 设计要点：
//   * 焊缝符号是「既有图元的组合」——指引线(Line) + 基准线(Line) + 基本符号(Line/Polyline/
//     Circle) + 辅助符号 + 补充符号 + 尺寸文本(Text)。**不新增 ElementType**，
//     全部经 toDrawingJson 单一入口序列化，与既有图元完全兼容。
//   * 所有符号线宽统一为 2d/3（d = 基本符号尺寸，默认 5mm），符合 GB/T 12212 比例。
//   * 指引线与基准线的组合规则（"手册第 13 章"）：箭头指向接头；基准线水平，指引线在
//     其一端与之相交；基本符号位于基准线下方为「箭头侧」，位于上方为「非箭头侧」，
//     位于基准线上为「双面焊」。尺寸标注：尺寸（焊脚 k / 坡口深 s）写在符号左侧，
//     长度 l、间距 e、段数 n 写在符号右侧。
//   * buildWeldSymbol 为纯几何生成器（图纸毫米，世界坐标），无头可测；渲染层只负责
//     把这些线段/文本落到屏幕，与 Gdt / SurfaceRoughness 同一范式。
// ============================================================================
namespace SketchCAD {

// 基本符号类型（GB/T 324 基本符号子集，顺序只增不改）
enum class WeldType {
    Fillet   = 0,   // 角焊缝        ▽（直角三角形）
    VGroove  = 1,   // V 形焊缝      ∨
    IGroove  = 2,   // Ⅰ 形焊缝      ‖（两条竖平行线）
    Bevel    = 3,   // 单边 V 形焊缝  ∟（弯折线）
    UGroove  = 4,   // U 形焊缝      ∪
    Spot     = 5,   // 点焊          ○
    Seam     = 6,   // 缝焊          ⌐（带圆弧的平行短线）
    Plug     = 7,   // 塞焊/槽焊     ⊓
    Count
};

// 辅助符号（GB/T 324 辅助符号）：平面 / 凸面 / 凹面
enum class WeldAux { None, Flat, Convex, Concave };

// 补充符号（GB/T 324 补充符号）：周围焊 / 现场焊 / 熔透焊
enum class WeldSupp { None, Around, Field, MeltThrough };

// 箭头侧（基本符号在基准线上的位置）
enum class WeldSide { Arrow, Other, Both };

// 焊缝尺寸（GB/T 324 尺寸标注）
struct WeldDims {
    double k = 0;   // 焊脚尺寸（角焊，mm）
    double s = 0;   // 坡口深度（坡口焊，mm）
    double l = 0;   // 焊缝长度（mm）
    double e = 0;   // 焊缝间距（断续焊，mm）
    int    n = 0;   // 焊缝段数（层数 / 段数）
};

// 焊缝符号完整描述
struct WeldSpec {
    WeldType type = WeldType::Fillet;
    WeldAux  aux  = WeldAux::None;
    WeldSupp supp = WeldSupp::None;
    WeldSide side = WeldSide::Arrow;     // 箭头侧（默认符号在基准线下方）
    WeldDims dims;
    double d = 5.0;                      // 基本符号尺寸（mm）；线宽 = 2d/3
    Vec3   anchor = Vec3(0, 0, 0);       // 指引线与基准线的交点（膝点）
    double rotationDeg = 0.0;            // 整体旋转角(度)，适配不同方向的接头
    QString layer = QStringLiteral("标注"); // 所属图层（与粗糙度符号一致）
};

// 生成一组焊缝符号图元（指引线 + 基准线 + 基本符号 + 辅助/补充符号 + 尺寸文本）。
// 全部为既有图元（Line/Polyline/Circle/Text），调用方经 addElementsQuiet 追加进模型即可；
// toDrawingJson 单一入口序列化，无需新增 ElementType。
std::vector<Element> buildWeldSymbol(const WeldSpec& spec);

// 尺寸标注文本（供预览 / 日志 / 文本图元）。左侧尺寸（k 或 s）+ 右侧长度/间距/段数。
QString weldDimText(const WeldSpec& spec);

// 类型名（UI 下拉 / 命令解析）
QStringList weldTypeNames();                       // 中文名列表
QString     weldTypeName(WeldType t);              // 中文名
WeldType    weldTypeFromName(const QString& name); // 中文名/英文别名 → 枚举

// 把 WeldSpec 序列化为规范 WELD 命令字符串（对话框确认时经 CommandInterpreter 执行）。
QString weldCommandString(const WeldSpec& spec);

} // namespace SketchCAD
