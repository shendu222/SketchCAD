#pragma once
#include <QPageSize>
#include <QPageLayout>
#include <QRectF>
#include <QMarginsF>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include "core/PlotStyle.h"
#include "SheetSpec.h"   // SheetSpec / SheetFormat：幅面规格 → 页面设置映射

namespace SketchCAD {

// ============================================================================
// 布局 / 视口 / 页面设置
// ----------------------------------------------------------------------------
// 布局体系（多布局 + 布局内多视口 + 视口独立比例）：
//   * PrintSettings 充当「页面设置」：页面参数 + 样式映射 + 视口集合。
//     视口为空 → 模型空间打印（整体图形直接出图，无需切换布局，历史行为不变）。
//   * PlotViewport 描述「模型空间的哪一块矩形区域(modelRect) 以何种比例映射到
//     图纸的哪一块矩形(paperRect)」；paperRect 为空 → 映射到整块可打印区
//     （MVP 兼容行为）。
//   * PlotLayoutDef = 布局名 + 一份完整页面设置；由 SketchModel 的布局表
//     （layouts_）持有并随图存档，实现多布局。
// 本头文件位于 core 层，不得引入 ui/ 依赖（QPdfWriter/QPrinter 等留在 PrintEngine）。
// ============================================================================

// 视口：模型空间中的一块矩形区域，及其到图纸的映射比例
struct PlotViewport {
    QString name;          // 视口名称（便于识别）
    QRectF  modelRect;     // 模型空间世界坐标(mm, y-up)；空矩形 → 整体图形范围
    double  scale = 1.0;   // 视口比例：视口内 1 个模型 mm 在图纸上占 scale mm；<=0 沿用布局整体比例
    double  rotationDeg = 0.0;
    // 图纸目标区（mm）：相对可打印区左上角、y 向下；空矩形 → 整块可打印区（MVP 行为）。
    // 视口平铺：布局内多个视口各占 paperRect 一块，实现矩形排布。
    QRectF  paperRect;
    // per-viewport 冻结：仅在该视口内不输出的图层名单（其他视口/模型空间正常输出）。
    // 序列化键 "frozenLayers"（缺键 → 空 = 旧档行为不变，逐字节等价改造前）。
    QStringList frozenLayers;

    // 该视口内某图层是否被冻结（只读裁决；渲染侧 PlotRenderer 消费）
    bool layerFrozenIn(const QString& layer) const { return frozenLayers.contains(layer); }

    QJsonObject toJson() const;
    static PlotViewport fromJson(const QJsonObject& o);
};

// ============================================================================
// 打印戳记（plot stamp）：可选叠加在页面边角的信息栏。
//   * 默认 enabled=false → 出图路径零绘制调用（旧行为逐字节不变）；
//   * 字段开关独立（图名/日期/时间/布局名/比例/文件名），字体与位置可配；
//   * 序列化键 "stamp" 挂在 PrintSettings 下，缺键 → 默认（关闭）。
// ============================================================================
struct PlotStamp {
    bool   enabled = false;       // 总开关（默认关闭）
    bool   showDrawingName = true;   // 图名（drawingPath 基名；无路径省略该行）
    bool   showDate = true;
    bool   showTime = true;
    bool   showLayoutName = true;
    bool   showScale = true;
    bool   showFileName = true;      // 完整文件路径（无路径省略该行）
    QString fontFamily;              // 空 = 应用默认字体
    double textHeightMm = 2.5;       // 纸面文字高（mm）
    int    position = 3;             // 0=左上 1=右上 2=左下 3=右下（相对可打印区角）

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);   // 缺键逐项回落默认（enabled 缺 → false）
};

// 当前设计把「布局」直接由 PrintSettings 表达，此处仅提供一个构造整体图形单视口的
// 便捷工厂，供 PrintSettings::defaults() 使用。
struct PlotLayout {
    static PlotViewport fullModelViewport() {
        PlotViewport vp;
        vp.name = QStringLiteral("模型空间");
        vp.modelRect = QRectF();   // 空 → 使用整体范围
        vp.scale = 1.0;
        return vp;
    }
};

// ============================================================================
// 页面设置（原定义于 ui/PrintEngine.h，为使 core 层 SketchModel 可持有布局表，
// 下沉至 core/Layout.h；字段与语义不变）
// ============================================================================
struct PrintSettings {
    QPageSize pageSize = QPageSize(QPageSize::A4);
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    QMarginsF marginsMm = QMarginsF(10.0, 10.0, 10.0, 10.0);  // 左/上/右/下 (mm)
    double scale = 1.0;          // 自定义比例：1.0 = 1:1（模型 mm → 图纸 mm）
    // 打印线宽比例：打印时对图元线宽(mm)的整体缩放。<1 变细（默认 0.5，细一档，避免 A4/A3
    // 上 0.5~2.0mm 的国标屏幕线宽印出来过粗失真）；1.0 = 按图纸 mm 原值输出。
    double lineWidthScale = 0.5;
    bool   fitToPage = true;     // 自适应铺满可打印区（优先于 scale）
    bool   centerH = true;       // 水平居中
    bool   centerV = true;       // 垂直居中
    bool   drawBorder = true;    // 绘制可打印区边框（便于预览对齐）
    // 黑白出图总开关：true 时若无自定义样式表，自动套用 GB 预设（按线型分粗/细线宽 + 纯黑）。
    // 用于「黑白打印机 / 黑白 PDF」一键出图，确保线宽分组效果符合 GB/T 14665。
    // 序列化键 "monochrome"（缺省 false → 旧档行为不变）。
    bool   monochrome = false;
    PlotStyleTable plotStyle;    // 简化 CTB 映射（灰度/线宽/抖动）
    QList<PlotViewport> viewports; // 空 = 模型空间打印；非空 = 按视口布局
    PlotStamp stamp;             // 打印戳记（默认关闭 → 旧行为不变；序列化键 "stamp"，缺键回落默认）

    QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);
    static PrintSettings defaults();

    // 渲染前把「黑白开关 + 样式表」解析为最终样式表：
    //   monochrome=true 且 plotStyle 为空 → 套用 GB 预设（按当前线宽组）；
    //   否则原样返回 plotStyle（自定义表优先，黑白总开关不覆盖用户表）。
    PlotStyleTable effectivePlotStyle() const;
};

// 布局定义：布局名 + 一份完整页面设置（含视口集）。由 SketchModel 布局表持有。
struct PlotLayoutDef {
    QString name;
    PrintSettings settings;

    QJsonObject toJson() const;
    static PlotLayoutDef fromJson(const QJsonObject& o);
};

// 纯函数：解析某视口最终出图比例（模型 mm → 图纸 mm）。
// 与 PrintEngine::render 的历史公式逐字节同源：
//   vp.scale > 0 → 视口独立比例（1:2 出图 = 0.5，2:1 出图 = 2.0）；
//   否则 fitToPage → 适配可打印区（留 2% 边距），再否则用布局整体 scale。
double viewportScaleMm(const PrintSettings& s, const PlotViewport& vp,
                       const QRectF& mb, double printW, double printH);

// ============================================================================
// 幅面规格 → 页面设置（纸张 + 方向）的**唯一入口**
// ----------------------------------------------------------------------------
// 历史缺陷：打印预览 / 导出 PDF 的页面设置来自 MainWindow::currentPrintSettings_
// （默认构造 = A4 纵向）或零布局导出时临时 new 的 PrintSettings（同样 A4 纵向），
// 从不读取新建图纸时用户选择的幅面与方向 → 「新建 A4 横向，打印/PDF 却出纵向」。
// 现在所有「页面从图纸幅面派生」的站点（新建/打开同步、零布局导出、COM 批出图、
// 新建布局默认页）一律走这两个函数，杜绝各处默认值漂移。
// ============================================================================

// SheetFormat → QPageSize 标准页 id（A0~A5 一一对应；越界回落 A4）
QPageSize::PageSizeId pageSizeIdForSheet(SheetFormat f);

// 把幅面规格写进页面设置的纸张与方向（**只动这两个字段**：边距/比例/样式/视口不动）。
// spec 非法 format 时保持 s 原样（不猜）。
void applySheetSpecToPage(const SheetSpec& spec, PrintSettings& s);

} // namespace SketchCAD
