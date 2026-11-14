#ifndef TOLERANCE_DB_H
#define TOLERANCE_DB_H

#include <QString>

namespace SketchCAD {

// ============================================================
// ISO 公差数据库（GB/T 1800.1 / ISO 286-1）
// ============================================================
// 提供 IT01~IT18 标准公差数值与轴(a~zc)/孔(A~ZC)基本偏差查询，
// 供 GD&T 标注、尺寸公差注写与 ComplianceChecker 共用。
// 数值依据《机械制图手册（第四版）》第 6 章「极限与配合」提取结果，
// 以 ISO 286-1 / GB/T 1800.1 基本偏差公式实现（即手册数表的生成规则），
// 因此 standardTolerance / deviation 返回值与手册数表完全一致（如 30mm/IT7 = 0.021mm）。
//
// 设计约束（同本工程其它"大表"）：
//   * 禁止巨型静态聚合初始化（规避 GCC 13.1 静默崩溃）——基本偏差表通过 builder lambda 注册；
//   * 不替换既有 DiagBundle 诊断（本类为纯查询，无副作用、不触碰诊断管线）。

// IT 公差等级：IT01, IT0, IT1..IT18（枚举序即等级序号，0=IT01 … 19=IT18）
enum class ITGrade {
    IT01, IT0, IT1, IT2, IT3, IT4, IT5, IT6, IT7, IT8,
    IT9, IT10, IT11, IT12, IT13, IT14, IT15, IT16, IT17, IT18
};

// 配合要素：孔 / 轴
enum class FitMember { Hole, Shaft };

// 上下偏差（mm，带符号）。例如 H7 孔对 30mm：{upper=+0.021, lower=0}
struct Deviation {
    double upper = 0.0;   // 上偏差 (ES / es)
    double lower = 0.0;   // 下偏差 (EI / ei)
};

class ToleranceDB {
public:
    // IT 标准公差（mm）：basicSizeMm 为基本尺寸(mm)；返回该尺寸段、该等级的标准公差。
    // 实现：取基本尺寸所在尺寸段的几何平均值 D，i=0.45·D^(1/3)+0.001·D(μm)，
    // IT=等级系数·i（IT5~IT18）或线性公式（IT01/IT0/IT1）/几何插值（IT2~IT4），
    // 结果按手册数表取整（IT5+ 取整到整数 μm，IT01~IT1 取 0.1μm）→ 返回 mm。
    static double standardTolerance(double basicSizeMm, ITGrade grade);

    // 基本偏差（mm）：letter 为字母（大小写均可，轴 a..zc / 孔 A..ZC；js/JS 对称）；
    // holeOrShaft 决定按孔或轴计算。返回 {upper(上偏差), lower(下偏差)}，符号按 ISO 约定。
    //   H 孔：下偏差 EI=0；h 轴：上偏差 es=0；JS/JS：±IT/2。
    static Deviation deviation(const QString& letter, double basicSizeMm,
                               ITGrade grade, FitMember holeOrShaft);

    // 便捷：由偏差与基本尺寸求上下极限尺寸(mm)。
    static inline double upperLimit(double basicSizeMm, const Deviation& d) {
        return basicSizeMm + d.upper;
    }
    static inline double lowerLimit(double basicSizeMm, const Deviation& d) {
        return basicSizeMm + d.lower;
    }
};

} // namespace SketchCAD

#endif // TOLERANCE_DB_H
