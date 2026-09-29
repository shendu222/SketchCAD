#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <vector>
#include "Element.h"

namespace SketchCAD {

// ============================================================================
// 单位（UNITS）与图纸界限（LIMITS）
// ----------------------------------------------------------------------------
// 单位不变式：**模型内部几何一律以 mm 存储**（图纸坐标系 = mm），单位设置只定义
// 「显示与输入的换算口径」，不改变任何已有几何。跨格式交换的换算只发生在导入/
// 导出边界（DXF $INSUNITS 元数据），保证：
//   * 显示精度 = 显示层四舍五入，存储精度永不损失（mm 全精度）；
//   * 交换语义明确（文件头声明单位，导入方按元数据换算到 mm）。
// ============================================================================

// 长度单位（枚举只增不改：JSON/DXF 换算表按整数索引）
enum class LengthUnit { Mm = 0, Cm, M, Inch, Foot };
// 角度单位（DMS = 度分秒，仅显示格式化；输入一律十进制度）
enum class AngleUnit { Deg = 0, DMS, Rad, Grad };

// 单位设置（显示/输入口径）
struct UnitsMeta {
    LengthUnit length = LengthUnit::Mm;
    AngleUnit  angle  = AngleUnit::Deg;
    int lengthPrec = 2;              // 长度显示小数位 0..6
    int anglePrec  = 2;              // 角度显示小数位 0..6（DMS 时为秒的小数位）
    bool operator==(const UnitsMeta& o) const {
        return length == o.length && angle == o.angle &&
               lengthPrec == o.lengthPrec && anglePrec == o.anglePrec;
    }
    bool operator!=(const UnitsMeta& o) const { return !(*this == o); }
    QJsonObject toJson() const;
    static UnitsMeta fromJson(const QJsonObject& o);   // 缺键回落 mm/十进制度/2 位
};

// 图纸界限（LIMITS）：矩形范围 + 越界检查开关（坐标为模型 mm）
struct LimitsMeta {
    bool   checkOn = false;          // 开启后新增图元越界给警告
    double x1 = 0.0, y1 = 0.0;
    double x2 = 420.0, y2 = 297.0;   // 默认 A3 图幅（mm）
    bool contains(double x, double y) const {
        return x >= x1 && x <= x2 && y >= y1 && y <= y2;
    }
    bool isValid() const { return x2 > x1 && y2 > y1; }
    bool operator==(const LimitsMeta& o) const {
        return checkOn == o.checkOn && x1 == o.x1 && y1 == o.y1 &&
               x2 == o.x2 && y2 == o.y2;
    }
    bool operator!=(const LimitsMeta& o) const { return !(*this == o); }
    QJsonObject toJson() const;
    static LimitsMeta fromJson(const QJsonObject& o);  // 缺键回落 A3 + 不检查
};

// ---- 单位换算与展示名 ----
double   lengthFactorToMm(LengthUnit u);      // mm=1, cm=10, m=1000, in=25.4, ft=304.8
QString  lengthUnitName(LengthUnit u);        // 毫米 / 厘米 / 米 / 英寸 / 英尺
QString  lengthUnitShort(LengthUnit u);       // mm / cm / m / in / ft
QString  angleUnitName(AngleUnit u);          // 十进制度 / 度分秒 / 弧度 / 百分度
QString  angleUnitShort(AngleUnit u);         // ° / DMS / rad / gon

// DXF $INSUNITS 标准码（AutoCAD）：0=无单位 1=英寸 2=英尺 4=毫米 5=厘米 6=米
int        insunitsCode(LengthUnit u);
LengthUnit lengthUnitFromInsunits(int code);  // 未知码回落 Mm

// ---- 显示格式化（存储 mm → 当前单位字符串，四舍五入到精度位）----
QString formatLengthMm(double mm, const UnitsMeta& u);        // 纯数值
QString formatLengthWithUnit(double mm, const UnitsMeta& u);  // 数值 + 短后缀（状态栏用）
// 角度：Deg/Rad/Grad 按精度取值；DMS 输出 12°34′56.8″（precision 作用于秒）
QString formatAngleDeg(double deg, const UnitsMeta& u);

// ---- 输入解析（当前单位 → mm）。支持显式后缀覆盖：mm/cm/m/in/"（英寸）/'（英尺）
// 无后缀按当前单位解释；解析失败 *ok=false 返回 0。
double parseLengthToMm(const QString& text, const UnitsMeta& u, bool* ok = nullptr);

// ---- 跨单位导入换算：把「以 from 为单位表达的图元集合」整体换算为 mm。
// 原点锚定（x' = x×factor）：单位换算改的是度量口径，不是图形位置，故以绝对原点
// 为基准缩放（与 setScaleRatio 的「内容中心锚定」不同——那是视觉等比，这是度量换算）。
// 覆盖全部长度量字段：pts/holes/radius/radiusY/dimTextHeight/mtextWidth/
// rowHeight/colWidths；不动 lineWidth（纸面量）与角度。
void scaleElementsToUnit(std::vector<Element>& elems, LengthUnit from);

} // namespace SketchCAD
