#pragma once
#include <vector>
#include <QString>
#include <QRectF>
#include "Element.h"
#include "Vec.h"
#include "SheetSpec.h"

namespace SketchCAD {

class SketchModel;

// 法兰盘三面图（主视图 / 俯视图 / 左视图）参数化模板。
//
// 几何结构严格取自附图（带颈平焊法兰，SO 型）：
//   一个圆形盘体，中心有一个上凸的**凸台 / 短颈**，颈部车出一道**正面锥面**（图中高光弧，
//   即 gasket face 的密封水线过渡），法兰盘面上均布 n 个**通孔**（螺栓孔），
//   中心为通孔（bore）。四孔法兰的孔位按 45° 错开布置（附图即为此姿态）。
//
// 所有尺寸单位：**毫米（mm）**。默认值取公称尺寸 DN100（PN16）一档，
// 与 GB/T 9115.1 / HG/T 20592 带颈平焊法兰系列的 D/K/L/C/n/螺纹规格对应。
//
// 结构不变式（无论参数如何变化都必须保持）：
//   1. 带颈模式（hasHub=true）下：盘体厚度 C < 颈部凸台高度 hubHeight，凸台必须从盘面凸出，
//      否则退化成平板；
//   2. 内孔径 bore < 螺栓孔分布圆半径的一半，否则螺栓孔会切进中心孔；
//   3. 螺栓孔分布圆 Dbolts/2 ∈ (bore/2 + 螺栓孔径/2, outerD/2 − 螺栓孔径/2)，
//      即螺栓孔必须完全落在中心孔与盘体外缘之间的环带内。
//   4. 止口（凹槽）模式：recessD 必须 ∈ (bore, outerD)，且 recessDepth < 厚度，
//      即止口必须包住内孔、又不穿出法兰外缘。
// 违反任一条时 makeValid() 会自动收敛到最近的合法值，而不是画出错误几何。
//
// 两种法兰形态：
//   • 带颈平焊法兰（hasHub=true，默认）：盘体 + 上凸颈部凸台，见上文附图。
//   • 平板止口法兰（hasHub=false）：无颈部凸台，轴向总长 = 盘体厚度 C；
//     右端面可带止口凹槽（recessD / recessDepth），用于密封面对接（如题目中的 Ø100h7×5 止口）。
struct FlangeParams {
    // ---- 附图表格中的关键连接尺寸（可直接照抄 GB/HG 法兰尺寸表）----
    double outerD    = 210.0;   // 法兰外径 D (mm)
    double boltCircle= 170.0;   // 螺栓孔中心圆直径 K (mm) —— 即题目所称"螺栓孔分布圆直径"
    double boltHoleD =  18.0;   // 螺栓孔径 L (mm)
    int    boltCount =   4;     // 螺栓孔数量 n (个)
    double thickness =  16.0;   // 法兰厚度 C (mm)

    // ---- 内孔与颈部（附图可见的上凸凸台）----
    double boreD     = 110.0;   // 中心孔径 (mm)；DN100 配管外径 114.3，取 110 展示颈部环面
    double hubD      = 150.0;   // 颈部（凸台）外径 N (mm)；默认取表格法兰颈 N = 130 与
                                //   DN100 盘体间的中间值，使凸台在附图中清晰可见
    double hubHeight =  26.0;   // 颈部总高 = 盘体厚度 + 凸台高度 (mm)
    double hubChamfer=   3.0;   // 颈部顶端倒角 (mm)
    double boreChamfer=  2.0;   // 中心孔两端倒角 (mm)

    // ---- 尺寸公差（可选；非空时追加到对应尺寸标注文本之后）----
    //   例：boreTol="H8" → 中心孔标注 "⌀60H8"；thicknessTol="±0.2" → 厚度 "20±0.2"；
    //       recessTol="h7" → 止口外径 "⌀100h7"。留空则不显示公差。
    QString boreTol;        // 中心孔公差（如 "H8"）
    QString thicknessTol;   // 法兰厚度公差（如 "±0.2"）
    QString recessTol;      // 止口外径公差（如 "h7"）

    // ---- 形态切换（平板止口法兰）----
    bool   hasHub     = true;   // 是否带颈部凸台：true=带颈平焊法兰（默认）；
                                //   false=平板法兰，轴向总长=盘体厚度 C，无凸台外圆。
    double recessD    =  0.0;   // 止口（凹槽）外径 (mm)，0 表示无凹槽
    double recessDepth=  0.0;   // 止口深度（沿轴向，mm）
    int    recessFace =  1;     // 止口所在端面：1=右端面(+轴向，默认)；0=左端面(-轴向)。
                                //   当前几何实现仅右端面(1)，左端面为后续扩展位。

    // ---- 视图布置 ----
    double sheetW    = 297.0;   // 图纸宽 (mm)
    double sheetH    = 210.0;   // 图纸高 (mm)，默认 A4 横向
    double marginL   =  25.0;   // 左侧装订边（与图框一致，标题栏避让用）
    double marginC   =  10.0;   // 其余三边（与图框一致）
    bool   autoScale = true;    // 自动选取 1:1 / 1:2 / 1:2.5 … 标准比例
    double manualScale = 1.0;   // autoScale=false 时使用的绘制比例（图上:实物）
    double boltAngle0Deg = 0.0; // 第一个螺栓孔的起始角（标准数学角，0°=+X）。
                                // 四孔时 0° 即"上下左右"姿态；设为 45° 可得附图中
                                // 四孔错开 45° 的姿态。仅在 boltCount 能被 360 整除时
                                // 才会避免孔位重合，其余角度由 makeValid 原样保留。

    // ---- 标题栏避让 ----
    double titleBlockW = 180.0; // 标题栏宽
    double titleBlockH =  56.0; // 标题栏高
    double gap         =  18.0; // 视图之间的最小安全间隙（mm，图纸量）。
                                //   指主视图与侧视图（水平）之间、主视图与俯视图（竖直）之间的净间距；
                                //   其中"主视图—侧视图"间距取本值，竖直方向额外加 10 mm 容纳尺寸标注。

    double dimTextHeight = 3.5;  // 尺寸标注字高(mm)（GB/T 14665-2012 表3：A2/A3/A4 字母数字下限 3.5）

    // 图纸初始化（A4 横向 297×210，边距同 GB/T 14689）
    static FlangeParams forA4() { return FlangeParams(); }

    // 收敛到满足全部结构不变式的最近合法参数（并修正不合法的离散值）。
    void makeValid();

    // 结构不变式校验（makeValid 的判据，供测试直接断言）。
    // 返回空串表示合法，否则返回首个违规说明。
    QString validate() const;

    // 视图自动布置所需的计算量（供测试与说明文档使用）：
    double viewScale() const;      // 实际采用的绘制比例（图上:实物）
    QRectF viewsBounds() const;    // 三视图 + 视图名 在图纸坐标下的总包围盒
    QRectF availableArea() const;  // 扣除图框边距与标题栏避让区后的可用区
};

// 三面图生成结果。
struct FlangeViewsResult {
    std::vector<Element> elems;   // 全部图元（调用方自行 beginEdit + addElementsQuiet）
    double scale = 1.0;           // 采用的绘制比例
    QString scaleLabel;           // 比例标签，如 "1:2"
    bool   fits = false;          // 三视图（含视图名）是否完整落在可用区内
    QRectF bounds;                // 视图实际包围盒（图纸坐标）
    QRectF area;                  // 可用区（图纸坐标）
    // 便于测试断言的关键量（图纸坐标，单位 mm）
    double frontCx = 0, frontCy = 0;   // 主视图中心
    double topCx   = 0, topCy   = 0;   // 俯视图中心（位于主视图正下方，长对正）
    double sideCx  = 0, sideCy  = 0;   // 侧视图中心（位于主视图右侧，高平齐）
    double frontR  = 0;                // 主视图外圆半径（= D*scale/2）
    double profHalfA = 0;              // 侧/俯视图轴向半长（= hubHeight*scale/2）
    double profHalfD = 0;              // 侧/俯视图直径半高（= outerD*scale/2）
    int    dimCount = 0;               // 尺寸标注数量
    int    leaderCount = 0;            // 引线标注数量
};

// 生成法兰盘三面图。
// 返回的图元尚未写入模型；坐标系为图纸坐标（左下角原点、X 右、Y 上，单位 mm），
// 与 buildSheetFrame() 的图框 / 标题栏完全同一坐标系，可直接叠加。
FlangeViewsResult buildFlangeViews(const FlangeParams& p, const SketchModel* model);

// 视图名文本（"主视图" / "俯视图" / "左视图"），集中一处便于测试与本地化。
QString flangeViewTitle(int index);

} // namespace SketchCAD
