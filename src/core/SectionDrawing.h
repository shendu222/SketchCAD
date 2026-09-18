#pragma once
#include <vector>
#include <QString>
#include "Element.h"
#include "Section.h"
#include "Vec.h"
#include "SurfaceRoughness.h"

namespace SketchCAD {

// ===================== 参数化剖面视图绘制接口 =====================
//
// 原程序剖面能力只有「交互式填充」：先手工画好闭合轮廓，再经【修改>剖面填充】
// 把选中图元替换成 Hatch。缺少"由参数直接生成整幅剖视图"的可复用接口，
// 导致每张剖视图（如附图箱体的主视图全剖、A—A 环形断面）都要从零手拼图元。
//
// 本模块提供两层 API：
//   1. SectionViewBuilder —— 通用剖视图构建器（流式接口）：
//        addCutRegion()   声明剖切到的实体区域（自动生成 GB/T 4457.5 剖面线 Hatch）；
//        addVisible*()    剖切面后方可见轮廓（粗实线：线/圆/圆弧）；
//        addHiddenLine()  不可见轮廓（细虚线）；
//        addCenterLine()  轴线/中心线（点划线）；
//        addDim()         尺寸标注；addRoughness() 粗糙度符号；addLabel() 文本；
//        build()          一次输出全部图元（图层按前缀分组，便于 BOM 树控制显示）。
//   2. makeRingSection() —— 参数化环形断面图（附图 A—A 这类回转体横断面）：
//        给定内/外径与壁上小孔，一键生成轮廓 + 剖面线 + 中心线 + 直径标注。
//
// 所有坐标为图纸坐标（mm，Y 向上），与 buildSheetFrame() 同一坐标系。

struct SectionViewStyle {
    double hatchAngleDeg  = 45.0;   // 剖面线方向（GB/T 4457.5：与主要轮廓成 45°）
    double hatchSpacingMm = 3.0;    // 剖面线间距（GB：2~4mm；<=0 时按区域厚度自适应）
    double outlineWidthPx = 2.0;    // 可见轮廓线宽(px)
    double thinWidthPx    = 0.6;    // 剖面线/细线线宽(px)
    double textHeight     = 3.5;    // 文本字高(mm)
    SectionMaterial material = SectionMaterial::General;
    SectionExempt   exempt   = SectionExempt::None;
};

struct SectionViewResult {
    std::vector<Element> elems;
    int hatchCount = 0;             // 剖面区域数
    int dimCount   = 0;             // 尺寸标注数
};

class SectionViewBuilder {
public:
    explicit SectionViewBuilder(const QString& layerPrefix = QStringLiteral("剖视"));

    // ---- 参数 ----
    SectionViewBuilder& setStyle(const SectionViewStyle& s);
    SectionViewBuilder& setLayerPrefix(const QString& prefix);

    // ---- 剖切实体区（生成剖面线 Hatch）。autoOutline=true 时按边界多边形
    //      自动补画粗实线轮廓（适合直线边界区域；圆形边界请用 addVisibleCircle 精确画）。----
    SectionViewBuilder& addCutRegion(const std::vector<Vec3>& boundary,
                                     const std::vector<std::vector<Vec3>>& holes = {},
                                     bool autoOutline = false);
    // 矩形剖切区（常用捷径）
    SectionViewBuilder& addCutRect(const Vec3& lo, const Vec3& hi, bool autoOutline = true);

    // ---- 可见轮廓（粗实线）----
    SectionViewBuilder& addVisibleLine(const Vec3& a, const Vec3& b);
    SectionViewBuilder& addVisiblePoly(const std::vector<Vec3>& pts, bool closed);
    SectionViewBuilder& addVisibleCircle(const Vec3& c, double r);
    SectionViewBuilder& addVisibleArc(const Vec3& c, double r, double a0Deg, double a1Deg);

    // ---- 不可见轮廓（细虚线）----
    SectionViewBuilder& addHiddenLine(const Vec3& a, const Vec3& b);

    // ---- 轴线 / 中心线（点划线）----
    SectionViewBuilder& addCenterLine(const Vec3& a, const Vec3& b);

    // ---- 标注 ----
    SectionViewBuilder& addDim(DimensionType dt, const Vec3& a, const Vec3& b, const Vec3& pos,
                               const QString& text);
    SectionViewBuilder& addRoughness(const RoughnessSpec& spec);   // 图层自动改写为本构建器前缀
    SectionViewBuilder& addLabel(const Vec3& pos, const QString& text);  // 普通文本

    // ---- 产出 ----
    SectionViewResult build();

private:
    void push(Element e, const QString& layer, const QString& name, double w, LineStyle ls);

    QString prefix_;
    SectionViewStyle st_;
    SectionViewResult res_;
};

// ---- 参数化环形断面图（回转体横断面，附图 A—A 同型）----
struct RingSectionSpec {
    Vec3  center;                  // 断面圆圆心（图纸 mm）
    double outerD = 82.0;          // 外径（附图 Ø82）
    double innerD = 64.0;          // 内径（附图 Ø64H8）
    struct WallHole {
        double angleDeg = 0.0;     // 孔心方位角（0°=+X，逆时针）
        double d        = 7.0;     // 孔径（附图 3×Ø7）
    };
    std::vector<WallHole> wallHoles;   // 壁上通孔（剖面线自动避让）
    double hatchAngleDeg  = 45.0;
    double hatchSpacingMm = 3.0;
    double textHeight     = 3.5;
    double centerLineExtend = 5.0;     // 中心线超出外圆的长度(mm)
    QString outerDimText;              // 外径标注文本（如 "⌀82"；空=不标）
    QString innerDimText;              // 内径标注文本（如 "⌀64H8"；空=不标）
    QString layerPrefix = QStringLiteral("剖视");
};

// 生成环形断面图（轮廓 + 剖面线 + 中心线 + 可选直径标注）。结果统计写入 res（可空）。
std::vector<Element> makeRingSection(const RingSectionSpec& spec, SectionViewResult* res = nullptr);

} // namespace SketchCAD
