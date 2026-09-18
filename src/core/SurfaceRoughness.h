#pragma once
#include <vector>
#include <QString>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

// ===================== 表面粗糙度符号（GB/T 131-2006）=====================
//
// 附图（箱体零件图）中大量出现 Ra 数值标注（如 Ra 1.6 / 6.3 / 12.5）与右上角
// 「其余 √」——原程序没有任何粗糙度绘制能力，本模块补齐。
//
// 符号几何（按字高 h 比例派生，GB/T 131-2006 第 6 章）：
//   * 基本符号：两条与标注面成 60° 的细实线组成的 V 形（尖端在标注面上）；
//   * 去除材料：基本符号 + 顶部横线闭合为三角形；
//   * 不允许去除材料：基本符号 + 尖端处内接小圆；
//   * 完整符号（注写参数值）：右腿加长（H2≈3h），横线之上注写数值文本。
//
// 用法（与附图对应）：
//   * 孔壁 / 加工面：process=RemoveMaterial，value="Ra 1.6"；
//   * 右上角「其余」：value="其余"（画基本符号 + 文本）。
struct RoughnessSpec {
    Vec3  anchor;                            // 符号尖端锚点（落在被标注表面上）
    double textHeight = 3.5;                 // 字高 h(mm)，符号尺寸按比例派生
    bool  removeMaterial = true;             // true=去除材料(闭合三角)；false=不去除(尖端加圆)
    QString value;                           // 参数文本，如 "Ra 3.2"；非空时画完整符号(加长右腿+横线)
    double rotationDeg = 0.0;                // 整体旋转角(度)，适配不同方向的标注面（0=尖朝下、开口向上）
    QString layer = QStringLiteral("标注");   // 图层
};

// 生成一组粗糙度符号图元（细实线 + 文本），调用方追加进模型即可。
std::vector<Element> buildRoughnessSymbol(const RoughnessSpec& spec);

} // namespace SketchCAD
