#pragma once
#include <vector>
#include <QString>
#include <QRectF>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"

namespace SketchCAD {

class SketchModel;

// 法兰盘轮毂视图模板（参照用户提供的轮毂法兰盘零件图）参数化实现。
//
// 零件结构（阶梯回转体，轴线水平，单位 mm）：
//   * 右端**法兰盘**（外径 flangeD、厚 flangeT），盘上均布 boltCount 个**螺栓通孔**
//     （分布圆 boltBCD、孔径 boltD），另均布 locCount 个**定位孔**（分布圆 locBCD、孔径 locD，
//     仅在左视图表达，不进入剖切面）；
//   * 左端**轮毂圆筒**（外径 hubD），左端面带 C×45° 倒角；
//   * 中心**通孔** boreD 贯穿全长，左端带沉孔 cboreD × cboreDepth 深；
//   * 法兰根部与轮毂交接处 **内凹圆角** filletR。
//
// 视图表达（本模板逐项对应附图）：
//   * 主视图——全剖 A—A：剖切面过轴线并穿过一个螺栓孔（GB 惯例，孔在剖面处中断）；
//     剖面线由参数化剖面绘制接口（SectionViewBuilder）生成；上方注写「A—A」；
//   * 左视图——圆形视图：法兰/轮毂/沉孔/通孔同心圆 + 两圈分布圆（点划线）+ 孔 + 十字中心线，
//     并画出竖直剖切符号 A—A（字母注在视图上下两端）；
//   * 尺寸标注：左侧 4 道（Ø法兰/Ø轮毂/Ø沉孔/Ø通孔），下方 3 道（沉孔深/总长/法兰厚），
//     引线标注（螺栓孔规格 / 倒角 / 圆角），粗糙度（孔面 Ra / 法兰外圆 Ra / 右上角「其余」）；
//   * 技术要求文本块。
//
// 结构不变式（违反时 makeValid() 自动收敛，validate() 供测试断言）：
//   1. flangeD > hubD > cboreD > boreD > 0（径向从外到内单调）；
//   2. cboreDepth ∈ (0, totalL − flangeT]（沉孔不得伸进法兰段）；
//   3. chamfer ∈ [0, (hubD − cboreD)/2)（左端面倒角后仍有环带）；
//   4. filletR ∈ [0, min(flangeT, (flangeD − hubD)/2))（圆角落在法兰根部实体内）；
//   5. 螺栓孔/定位孔整圆落在 (boreD, flangeD) 环带内（各留 0.5mm 余量）。
struct FlangeHubParams {
    // ---- 径向尺寸 ----
    double flangeD  = 80.0;    // 法兰外径（附图 Ø80）
    double hubD     = 60.0;    // 轮毂外径（附图 Ø60）
    double cboreD   = 41.0;    // 左端沉孔径（附图 Ø41）
    double boreD    = 33.0;    // 中心通孔径（附图 Ø33）

    // ---- 轴向尺寸 ----
    double cboreDepth =  8.0;  // 沉孔深（附图 8）
    double flangeT    = 26.0;  // 法兰厚度（附图 26）
    double totalL     =136.0;  // 零件总长（附图 136）

    // ---- 过渡特征 ----
    double chamfer = 3.0;      // 轮毂左端倒角 C（附图 3×45°）
    double filletR = 4.0;      // 法兰根部内凹圆角 R（附图 R4）

    // ---- 螺栓孔（剖切面穿过其中一个）----
    int    boltCount    = 3;     // 数量（附图 3-Ø11.5）
    double boltD        = 11.5;  // 孔径
    double boltBCD      = 52.0;  // 分布圆直径
    double boltAngle0Deg= 90.0;  // 起始角（0°=+X 逆时针；90° 使首孔朝上，与剖切面对齐）

    // ---- 定位孔（仅左视图表达）----
    int    locCount    = 2;      // 数量（附图 2-Ø10）
    double locD        = 10.0;   // 孔径
    double locBCD      = 68.0;   // 分布圆直径（落于 (通孔, 法兰外缘) 环带内，留 0.5mm 余量）
    double locAngle0Deg= 150.0;  // 起始角

    // ---- 标注 ----
    QString roughBore = QStringLiteral("Ra 3.2");  // 中心孔面粗糙度（空=不标）
    QString roughFace = QStringLiteral("Ra 6.3");  // 法兰外圆粗糙度（空=不标）
    QString boreTol;                               // 中心孔公差（如 "H8"；空=不注）
    double  dimTextHeight = 3.5;                   // 尺寸字高（GB/T 14665 A4 下限 3.5）

    // ---- 图纸布置（默认 A4 横向）----
    double sheetW = 297.0;
    double sheetH = 210.0;
    double marginL = 25.0;
    double marginC =  5.0;
    bool   autoScale = true;
    double manualScale = 1.0;
    double gap = 15.0;              // 主视图与左视图最小间隙（mm，图纸量）
    double hatchAngleDeg  = 45.0;   // 剖面线方向
    double hatchSpacingMm = 3.0;    // 剖面线间距
    double titleBlockW = 180.0;     // 标题栏避让
    double titleBlockH = 56.0;

    static FlangeHubParams forA4() { return FlangeHubParams(); }

    // ---- 派生量（模型 mm）----
    double flangeR() const { return flangeD * 0.5; }
    double hubR()    const { return hubD    * 0.5; }
    double boreR()   const { return boreD   * 0.5; }
    double cboreR()  const { return cboreD  * 0.5; }
    double hubLen()  const { return totalL - flangeT; }   // 轮毂段轴向长
    double hubFlangeX() const { return totalL - flangeT; } // 轮毂—法兰交接面 x

    void makeValid();
    QString validate() const;
    double viewScale() const;
    QRectF viewsBounds() const;
    QRectF availableArea() const;
};

struct FlangeHubViewsResult {
    std::vector<Element> elems;     // 全部图元（图纸坐标 mm，Y 向上；未写入模型）
    double scale = 1.0;
    QString scaleLabel;
    bool   fits = false;
    QRectF bounds;
    QRectF area;
    // 视图锚点（图纸 mm，便于测试断言）
    double frontCx = 0, frontCy = 0;   // 主视图轴线中点（x = 零件轴向中点，y = 轴线）
    double sideCx  = 0, sideCy  = 0;   // 左视图圆心
    double frontHalfL = 0;             // 主视图轴向半长（= totalL*scale/2）
    double sideR      = 0;             // 左视图最大圆半径（= flangeD*scale/2）
    int    hatchCount = 0;             // 剖面区域数（上/下两半 = 2）
    int    dimCount   = 0;             // 尺寸标注数（含引线）
    int    roughnessCount = 0;         // 粗糙度符号组数
    int    cutSymbolCount = 0;         // 剖切符号图元数
    int    boltCircleCount = 0;        // 左视图孔圆总数（螺栓孔 + 定位孔）
};

// 生成法兰盘轮毂视图（图元未写入模型；坐标系与 buildSheetFrame() 一致，可直接叠加）。
FlangeHubViewsResult buildFlangeHubViews(const FlangeHubParams& p, const SketchModel* model);

// 视图名（"主视图（全剖 A—A）" / "左视图"）。
QString flangeHubViewTitle(int index);

} // namespace SketchCAD
