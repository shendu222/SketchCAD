#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include <QString>
#include <QRectF>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"

namespace SketchCAD {

class SketchModel;

// 轴承座三面图（主视图 / 俯视图 / 左视图·全剖）参数化模板。
//
// 几何结构（标准「底板 + 支承板 + 圆筒」式轴承座，单位 mm）：
//   * 底板：长方体 baseL(X) × baseW(Z) × baseThk(Y)，底面 y=0；两端各一个沿 X 的
//     U 形开口地脚槽（宽 footSlotWd、深 footSlotDepth，槽底为半圆）。
//   * 支承板：位于底板中央、与底板等宽(Z)的梯形立板，底宽 webBotW(X)、顶宽自动取到
//     与轴承孔底相切处（顶面 y = bossCy − ringD/2，顶面两角点正好落在圆筒外圆上）。
//   * 圆筒（轴承孔）：轴线沿 Z（宽度方向）的圆柱，外径 bossD、轴向长 bossLen；
//     内孔为台阶孔——自前端面(z=+bossLen/2)向内深 ringDepth 为 ringD，其余为 boreD 通孔。
//
// 三视图（第一角投影：俯视图在主视图下方、左视图在主视图右方）：
//   * 主视图（沿 −Z）：底板矩形 + 地脚孔(虚线) + 支承板斜边 + 圆筒外圆可见弧
//     + 轴承孔 ØringD + 台阶孔 ØboreD + 中心线。
//   * 俯视图（沿 −Y）：底板矩形 + 两个长圆地脚孔 + 支承板矩形/顶面边 + 圆筒矩形
//     + 台阶孔(虚线) + 中心线。
//   * 左视图（沿 X，全剖）：剖切平面过圆筒轴线（零件对称面，按 GB 可省略剖切标注），
//     断面用 GB/T 4457.5-2013 的 45° 细实线剖面线填充。
//
// 结构不变式（无论参数怎么改都必须成立，makeValid() 自动收敛）：
//   1. bossD > ringD > boreD > 0；
//   2. bossCy = overallH − bossD/2，且 webTopY = bossCy − ringD/2 > baseThk
//      （轴承孔底高于底板顶面，支承板有可见高度）；
//   3. bossLen ≤ baseW（圆筒不探出底板宽度）；
//   4. U 形地脚槽整体落在底板内，且不切入支承板正投影。
struct BearingParams {
    // ---- 圆筒（轴承孔），轴线沿宽度方向 Z ----
    double bossD   = 130.0;   // 圆筒外径 Ø130
    double ringD   =  96.0;   // 轴承孔（内孔）Ø96
    double boreD   =  30.0;   // 后端台阶通孔 Ø30
    double bossLen = 100.0;   // 圆筒轴向长度（沿 Z）

    // ---- 底板 ----
    double baseL    = 240.0;  // 底板长（沿 X）
    double baseW    = 160.0;  // 底板宽（沿 Z）
    double baseThk  =  20.0;  // 底板厚（沿 Y）
    double overallH = 180.0;  // 总高（底板底面 → 圆筒顶）

    // ---- 支承板 ----
    double webBotW  = 105.0;  // 支承板底宽（沿 X），顶宽自动求

    // ---- 地脚 U 形开口槽（底板两端，自端面向内开通）----
    double footSlotWd    = 55.0;  // 槽宽（沿 Z）
    double footSlotDepth = 36.0;  // 槽深（沿 X，自底板端面向内）

    // ---- 视图布置 ----
    double sheetW  = 297.0;   // 图纸宽 (mm)
    double sheetH  = 210.0;   // 图纸高 (mm)，默认 A4 横向
    double marginL =  25.0;   // 左侧装订边
    double marginC =  10.0;   // 其余三边
    bool   autoScale   = true;
    double manualScale = 1.0;
    double gap          = 18.0;  // 视图间最小间隙（mm）
    double dimTextHeight = 3.5;  // 尺寸字高 (mm)（GB/T 14665-2012 表3：A4 字母数字 3.5）

    // 标题栏避让
    double titleBlockW = 180.0;
    double titleBlockH =  56.0;

    static BearingParams forA4() { return BearingParams(); }

    // ---- 派生几何量（模型坐标，mm）----
    double bossR()   const { return bossD * 0.5; }               // 圆筒外半径
    double ringR()   const { return ringD * 0.5; }               // 轴承孔半径
    double boreR()   const { return boreD * 0.5; }               // 台阶孔半径
    double bossCy()  const { return overallH - bossD * 0.5; }    // 圆筒轴心高度
    double webTopY() const { return bossCy() - ringD * 0.5; }    // 支承板顶面（与孔底相切）
    double slotCx()  const { return baseL * 0.5 - footSlotDepth; } // 槽底半圆圆心 X
    // Ø96 段深度（自前端面向内），其余为 Ø30
    double ringDepth() const { return std::min(bossLen * 0.55, bossLen - 4.0); }
    double zStep()     const { return bossLen * 0.5 - ringDepth(); } // 台阶位置（Z）
    // 支承板顶面半宽：顶面两角点落在圆筒外圆上 → √(R²−r²)
    double webTopHalf() const {
        const double v = bossR() * bossR() - ringR() * ringR();
        const double m = v > 0.0 ? std::sqrt(v) : 0.0;
        return std::min(webBotW * 0.5, m);
    }

    void makeValid();
    QString validate() const;
    double viewScale() const;
    QRectF viewsBounds() const;
    QRectF availableArea() const;
};

// 三面图生成结果（与 FlangeViewsResult 同构）。
struct BearingViewsResult {
    std::vector<Element> elems;
    double scale = 1.0;
    QString scaleLabel;
    bool   fits = false;
    QRectF bounds;
    QRectF area;
    double frontCx = 0, frontCy = 0;   // 主视图中心（图纸 mm）
    double topCx   = 0, topCy   = 0;   // 俯视图中心（主视图正下方，长对正）
    double sideCx  = 0, sideCy  = 0;   // 左视图中心（主视图右侧，高平齐）
    double bossR   = 0;               // 主视图圆筒外圆半径（图纸 mm）
    double baseHalfL = 0;             // 底板半长（图纸 mm）
    double baseHalfW = 0;             // 底板半宽（图纸 mm）
    int    dimCount = 0;
};

// 生成轴承座三面图（图元尚未写入模型；图纸坐标系，单位 mm，Y 轴向上）。
BearingViewsResult buildBearingViews(const BearingParams& p, const SketchModel* model);

// 视图名（"主视图" / "俯视图" / "左视图"）；按出图习惯不绘制在图纸上。
QString bearingViewTitle(int index);

} // namespace SketchCAD
