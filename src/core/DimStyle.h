#pragma once
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <vector>
#include <utility>
#include "Vec.h"
#include "Element.h"
#include "DimensionStandard.h"

// 尺寸标注的「几何生成器」——同时支持 GB/T 4458.4-2003 与 ISO 129-1:2018。
// 把一个 Dimension 图元展开成：尺寸界线 + 尺寸线 + 箭头 + 数字位置/朝向。
//
// 为什么单独一层而不是直接在 QGraphicsItem 里画：
//   1) 国标/ISO 的量（箭头长≈6d、界线超出 2~3mm、字高 3.5mm）都是「图纸毫米」，
//      属于制图规范而不是绘制技巧，放在核心层才能被自测覆盖、被 DXF 导出复用；
//   2) 渲染层只负责把这些线段/多边形/文字落到屏幕，两者职责不纠缠。
namespace SketchCAD {
namespace dim {

// 国标/ISO 名义尺寸要素（单位 mm，按粗实线宽 d≈0.6mm 取值）
struct Style {
    DimensionStandardConfig stdConf = DimensionStandardConfig::gbtDefault();

    double arrowLen   = 3.5;    // 箭头长 ≈ 6d
    double arrowWidth = 1.1;    // 箭头宽 ≈ 2d（实心闭合箭头，尖端与尺寸界线接触）
    double extBeyond  = 2.5;    // 尺寸界线超出尺寸线终端 2~3mm
    double extGap     = 0.0;    // 界线起点与轮廓的间隙（国标为接触，留常量便于调整）
    double textHeight = 3.5;    // 数字字高（GB/T 14691 常用号数）
    double textGap    = 0.8;    // 数字底边与尺寸线的间距
    double defOffset  = 12.0;   // 未指定尺寸线位置时相对测量线的默认偏置
    double arcSeg     = 4.0;    // 角度尺寸弧的折线近似步长（度）

    // 绘图比例 factor（图形:实物），默认 1.0。
    // 模型里 Dimension 的 pts 存的是**图纸坐标**；而标注数字永远等于**实物尺寸**
    // （GB/T 14690：比例只改变图形大小，不改变标注数字）。故数值 = 图上实测 / drawingScale。
    // 注意：箭头长、字高、界线余量、偏置都是「图纸上的固定量」，**不**随此值变化。
    double drawingScale = 1.0;

    // 辅助访问器
    DimensionStandard standard() const { return stdConf.standard; }
    ArrowForm arrowForm() const { return stdConf.arrowForm; }
    TextOrientation textOrientation() const { return stdConf.textOrientation; }
    ToleranceStyle toleranceStyle() const { return stdConf.toleranceStyle; }
};

// 实心/空心/开口/斜线/圆点等箭头统一描述：tip 为尖端，dirDeg 为「由尾指向尖」的方向角
struct Arrow { Vec3 tip; double dirDeg = 0; };

struct Geom {
    bool valid = false;
    std::vector<std::pair<Vec3, Vec3>> ext;    // 尺寸界线（细实线）
    std::vector<std::pair<Vec3, Vec3>> line;   // 尺寸线 / 指引线（细实线）
    std::vector<Vec3> arcPoly;                 // 角度/弧长标注的圆弧尺寸线（折线近似）
    std::vector<Arrow> arrows;
    QString label;                             // 尺寸数字（含 R / ⌀ / ° / ⌒ 前后缀）
    QString tolLabel;                          // 公差注写（如 ±0.1、上/下偏差），与主数字分开定位
    Vec3    textPos;                           // 数字外框中心
    double  textRotDeg = 0.0;                  // 0=水平；90=字头朝左（竖直尺寸）
    double  measured = 0.0;                    // 图上实测值（图纸坐标，用于定位/小尺寸判定）
    double  value = 0.0;                       // 标注数值 = measured / drawingScale（实物尺寸）
    bool    textOnShelf = false;               // 文字是否放在引出的水平基线上（小尺寸半径/引线）
};

QString diameterSign();                        // "⌀"（GB/ISO 直径符号）
QString arcLengthSign();                       // "⌒"（ISO 129 弧长符号）
QString formatValue(const Element& e, double measured);
QString formatTolerance(const Element& e, double nominal, const Style& st = Style());
Geom    build(const Element& e, const Style& st = Style());

// 计算标注文字按标准应取的旋转角（避免倒置、处理水平/ISO 对齐策略）
double textRotationForLine(double lineAngleDeg, TextOrientation policy);

// ---- 命名标注样式（DimStyle 表）----
// 一图多标准：GB/T 4458.4 与 ISO 129-1 在箭头/字高/文字朝向/公差格式上不同，
// 需在同一张图上并存多套命名样式。表以「name → Style」多实例化，
// 图元经 dimStyleName 按名引用（引用式，对标 blockTable / textStyleName 范式）：
// 改样式定义 → 引用它的全部标注实时联动；无 id 悬空问题。
// dimStyleName 为空串时用「当前样式」（SketchModel 维护 currentDimStyle_）。
struct DimStyleEntry {
    QString name;             // 样式名（唯一键）；默认表首项固定名 "Standard"
    Style   style;
};

// 序列化（随图存档；旧档无 dimStyles 键 → SketchModel 自动生成默认 "Standard" 一条）
QJsonObject dimStyleEntryToJson(const DimStyleEntry& e);
DimStyleEntry dimStyleEntryFromJson(const QJsonObject& o);

} // namespace dim
} // namespace SketchCAD
