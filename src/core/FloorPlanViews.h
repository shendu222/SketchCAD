#pragma once
// =====================================================================
// FloorPlanViews —— 参数化户型平面图制图模板（建筑装修专业）
// =====================================================================
// 依据附图（C户型 101M² 三房两厅两卫装饰平面图）补充的参数化绘制能力。
// 与机械制图模板（FlangeViews / HousingViews / FlangeHubViews）同构：
//   Params(可调参数 + 派生量 + 合法性收敛) + build() + Result(结果统计)。
//
// 本模块补齐的绘图接口缺口（原程序只有机械制图要素，缺建筑要素）：
//   1. 墙体表达 —— 双线墙体 + 剪力墙涂黑（复用 Hatch + SectionExempt::SmallAreaSolid
//      的 SolidFill 通道，涂黑色 = 图元颜色），开门/窗洞口自动断开；
//   2. 门 —— 门扇线 + 90° 开启弧（Arc），推拉门为双错位滑扇；
//   3. 窗 —— 洞口内三条平行细线（建筑平立面惯例）；
//   4. 家具图块 —— 床/沙发/餐桌椅/洁具/衣柜/洗衣机等由直线/圆/椭圆/矩形组合；
//   5. 建筑尺寸链 —— 斜短线(45°)起止符号的连续标注链。原 Dimension 体系只有
//      箭头/圆点终端且 arrowForm 由标准全局决定、不可逐元素覆盖，故尺寸链以
//      直线 + 文字 + 斜短线图元组合绘制（见 buildHChain / buildVChain）；
//   6. 装饰图签 —— 右侧整幅装饰公司图签（设计单位/建设单位/工程名称/图名等）。
//
// 坐标体系：模型坐标 = 户型实际尺寸(mm)，原点在户型外框左下角，Y 轴向上；
//           图纸坐标 = 纸面 mm，原点在图纸左下角。build 内部经 X()/Y() 变换。
// 单位：全部毫米（mm）。比例默认 1:50（建筑平面图常用）。

#include <QString>
#include <QRectF>
#include <QColor>
#include <vector>
#include "Element.h"

namespace SketchCAD {

class SketchModel;

// ----------------------------- 参数 -----------------------------
struct FloorPlanParams {
    // ---- 户型总尺寸（外框轴线尺寸，模型 mm）----
    double outerW = 9200.0;      // 总开间（附图 9200）
    double outerH = 11460.0;     // 总进深（附图 11460）

    // ---- 墙体 ----
    double shearWallT = 200.0;   // 外墙/剪力墙厚（涂黑表达）
    double partWallT  = 100.0;   // 内隔墙厚（双线表达）
    double doorW      = 900.0;   // 房门洞口宽（门扇/开启弧半径同值）
    double bathDoorW  = 800.0;   // 卫生间门宽
    double slideDoorW = 1500.0;  // 阳台推拉门宽
    double winDefaultW = 1500.0; // 默认窗宽（预留：洞口未显式给宽时使用；当前布局均显式给宽）

    // ---- 文字/标注（纸面 mm）----
    double labelTextH  = 3.5;    // 房间名字高
    double dimTextH    = 2.5;    // 尺寸链数字字高
    double titleTextH  = 3.5;    // 图签文字字高

    // ---- 图纸（A3 横放）----
    double sheetW = 420.0;
    double sheetH = 297.0;
    double margin = 10.0;        // 图框边距（建筑图惯例 10）
    double titleBlockW = 115.0;  // 右侧图签宽（通高）
    double dimBand = 22.0;       // 尺寸链占用的纸面带宽（平面图四周）

    // ---- 比例 ----
    bool   autoScale   = true;
    double manualScale = 50.0;   // manualScale 为分母：1:50 → scale = 1/50

    // ---- 开关 ----
    bool showFurniture = true;   // 绘制家具图块
    bool showDims      = true;   // 绘制四周尺寸链
    bool showLabels    = true;   // 绘制房间名标注

    // ---- 图签文本 ----
    QString company     = QStringLiteral("昊尚装饰 HAOSHANG DECORATION");
    QString client      = QStringLiteral("建设单位：金地景业");
    QString projectName = QStringLiteral("金地景域43-1101");
    QString drawingName = QStringLiteral("C户型101M²三房两厅两卫");
    QString drawScale   = QStringLiteral("1:50");   // 图签内标注的比例文本

    static FloorPlanParams forA3() { return FloorPlanParams(); }

    // ---- 派生量 ----
    double scaleDenom() const { return autoScale ? 50.0 : manualScale; } // 近似，build 内重算
    double paperW() const { return outerW / scaleDenom(); }
    double paperH() const { return outerH / scaleDenom(); }

    // 合法性收敛：非法/越界参数自动修正（保持可绘制）
    void makeValid();
    // 不变式检查：返回空串 = 合法
    QString validate() const;
};

// ----------------------------- 结果 -----------------------------
struct FloorPlanViewsResult {
    std::vector<Element> elems;   // 生成的全部图元（图纸坐标）
    double scale = 0.02;          // 实际采用比例（图纸 mm / 模型 mm）
    QString scaleLabel;           // 如 "1:50"
    bool   fits = false;          // 内容是否完整落在图框内
    QRectF bounds;                // 内容包围盒（图纸 mm）
    QRectF area;                  // 图框内可用区（图纸 mm）

    // 平面图在图纸上的锚点（图纸 mm）
    double planX0 = 0, planY0 = 0;    // 户型外框左下角
    double planW = 0, planH = 0;      // 外框纸面尺寸

    // 统计计数
    int wallRunCount    = 0;      // 墙段（划分洞口后的实墙段）数
    int pocheCount      = 0;      // 涂黑剪力墙区域数（Hatch SolidFill）
    int doorCount       = 0;      // 门（含推拉门）
    int windowCount     = 0;      // 窗
    int furnitureCount  = 0;      // 家具图块数
    int dimChainCount   = 0;      // 尺寸链条数
    int dimCount        = 0;      // 尺寸链中"分格"总数
    int labelCount      = 0;      // 房间名标注数
    int titleFieldCount = 0;      // 图签栏目数
};

// ----------------------------- 入口 -----------------------------
// 生成参数化户型平面图。model 仅用于套用默认样式（可 nullptr）。
FloorPlanViewsResult buildFloorPlanViews(const FloorPlanParams& p, const SketchModel* model);

} // namespace SketchCAD
