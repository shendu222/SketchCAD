#pragma once
#include <vector>
#include <QString>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

// ===================== 剖切符号与剖视图名（GB/T 17452 / GB/T 4458.6）=====================
//
// 附图（箱体零件图）的主视图两侧画有剖切符号（粗短位置线 + 剖视方向箭头 + 字母 A），
// 断面图上方注写视图名「A—A」——原程序缺失该能力（差距分析报告搁置项 T4），本模块补齐。
//
// 构成（GB/T 4458.6-2002 第 5 章）：
//   * 剖切位置线：两端各一段短粗线（长 6~10mm，粗实线），指示剖切面位置；
//   * 箭头：与位置线垂直、指向剖视观察方向，画在位置线外侧端部；
//   * 字母：大写字母（如 A）注写在箭头旁，断面图上方对应注写「A—A」。

enum class CutViewDir { Up, Down, Left, Right };   // 剖视方向（箭头指向 = 观察方向）

struct CutSymbolSpec {
    Vec3  markPoint;                // 位置线中点（剖切面与轮廓交点附近）
    double lineLen    = 8.0;        // 位置线长度(mm)，标准 6~10
    CutViewDir viewDir = CutViewDir::Up;  // 剖视方向
    double arrowLen   = 8.0;        // 箭杆长(mm)
    QString letter    = QStringLiteral("A");  // 字母（单字符）
    double textHeight = 5.0;        // 字母字高(mm)
    QString layer     = QStringLiteral("标注");
};

// 生成一端剖切符号（位置粗短线 + 箭头 + 字母）。
std::vector<Element> buildCutSymbolEnd(const CutSymbolSpec& s);

// 生成两端剖切符号（start/end 各一组；两端 viewDir 相同）。
std::vector<Element> buildCutAnnotation(const Vec3& start, const Vec3& end,
                                        CutViewDir dir, const QString& letter,
                                        double textHeight = 5.0,
                                        const QString& layer = QStringLiteral("标注"));

// 剖视图/断面图名称文本（如「A—A」），置于视图上方居中。
Element buildSectionViewLabel(const Vec3& center, const QString& letter,
                              double textHeight = 5.0,
                              const QString& layer = QStringLiteral("标注"));

} // namespace SketchCAD
