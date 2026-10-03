#pragma once
#include "Element.h"
#include "Vec.h"
#include <vector>
#include <QString>

namespace SketchCAD {

class SketchModel;

// ===================== 工厂模式：统一图元创建 =====================
// 原先图元创建逻辑分散在两处：
//   - CommandInterpreter::makeBase()  命令行入口
//   - GraphicsView::newElement()      鼠标交互入口
// 两者实现完全相同（设 type/name → applyDefaultStyle），违反 DRY。
// 此外，引导线(guides)的创建散落在各处，用裸 `Element g; g.type=...; g.pts=...;` 手写。
//
// ElementFactory 收敛所有创建逻辑：
//   - create()    统一入口，自动套用模型默认样式
//   - createLine/Circle/Arc/...  常见图元的便捷构造
//   - createGuide()  轻量引导线（预览用，不套样式）
//
// 新增图元类型时只需在此工厂中添加一个便捷方法，所有调用方自动获益。
class ElementFactory {
public:
    // 基础创建：设置类型与名称，套用模型默认样式（颜色/线宽/线型）
    static Element create(ElementType t, const QString& name, const SketchModel* model);

    // ---- 便捷构造：常见图元一行创建 ----
    static Element createLine(const Vec3& a, const Vec3& b, const QString& name, const SketchModel* model);
    static Element createLine(const Vec3& a, const Vec3& b, const SketchModel* model);  // 默认名"直线"
    static Element createCircle(const Vec3& center, double radius, const SketchModel* model);
    static Element createArc(const Vec3& center, double radius, double startDeg, double endDeg, const SketchModel* model);
    static Element createEllipse(const Vec3& center, double radiusX, double radiusY, double rotationDeg, const SketchModel* model);
    static Element createRect(const Vec3& a, const Vec3& b, const SketchModel* model);
    static Element createPolygon(const std::vector<Vec3>& pts, const SketchModel* model);
    static Element createSpline(const std::vector<Vec3>& pts, const SketchModel* model);
    static Element createPolyline(const std::vector<Vec3>& pts, const std::vector<bool>& arcFlags, bool closed, const SketchModel* model);
    // 边界（BOUNDARY / BPOLY）：用户交互定义的填充边界闭合环（可含内孔）。
    // pts=外环，holes=内孔（每个自动闭合、布尔减）；closed 恒 true；
    // rotation=剖面线方向角(默认45)，radius=剖面线间距(默认3)，
    // sectionMaterial/sectionExempt 走与 Hatch 一致的字段。REGION 由 name 分组，不在此构造。
    static Element createBoundary(const std::vector<Vec3>& pts,
                                 const std::vector<std::vector<Vec3>>& holes,
                                 const SketchModel* model);
    static Element createDimension(DimensionType dt, const SketchModel* model);
    // 文本图元（标题栏栏目、说明文字等）。文字高度(mm) 存于 radius 字段；
    // 颜色/线型/线宽走默认样式，调用方可再按需覆盖。
    static Element createText(const Vec3& pos, const QString& text, double heightMm,
                              const QColor& color = QColor(26, 29, 40));

    // 多行文字（MText）：pos=插入锚点(世界坐标)；其余多行属性（对齐/样式/宽度因子/行距/自动换行）
    // 经可选参数传入，未给时取 Element 默认（左中/无样式/宽因子1/行距1.3/不自动换行）。
    static Element createMText(const Vec3& pos, const QString& text, double heightMm,
                              const QColor& color = QColor(26, 29, 40),
                              int textAlign = 1, double widthFactor = 1.0,
                              const QString& styleName = QString(),
                              double lineSpacing = 1.3, double mtextWidth = 0.0);

    // 表格（Table/明细表）：topLeft=左上角锚点(世界坐标，行向下增长)。
    // rows/cols 钳到 [1,200]；列宽均分 30mm、单元格全空；字高 3.5、文字水平居中。
    // 单元格内容由 TableDialog 填充；网格线颜色/线型走模型默认样式。
    static Element createTable(const Vec3& topLeft, int rows, int cols, const SketchModel* model);

    // 多重引线（MLeader）：tip=箭头附着点；elbow=折点（可与 tip 同点表示直线引线）；
    // textAnchor=文字框基线起点；text=多行内容（\n 分行）；heightMm=文字高度；
    // landingMm=基线长度（<=0 无基线）；arrowLenMm=箭头长。
    // pts 依次存 [tip, elbow, textAnchor]（三点，elbow==tip 时退化为两点）。
    static Element createMLeader(const Vec3& tip, const Vec3& elbow, const Vec3& textAnchor,
                                 const QString& text, double heightMm = 3.5,
                                 double landingMm = 0.0, double arrowLenMm = 3.5,
                                 const SketchModel* model = nullptr);

    // 形位公差框格（Fcf，GB/T 1182）：anchor=框格左下角；symbol=几何特征符号（FcfSymbol 整数）；
    // tolerance=公差值文本（可空）；datums=基准字母组；heightMm=框格高/字高。
    // modifer=周边界/最大实体(M)/最小实体(L)/自由状态(S)/投影(P)修饰符（可空）。
    static Element createFcf(const Vec3& anchor, int symbol, const QString& tolerance,
                             const std::vector<QString>& datums, double heightMm = 7.0,
                             const QString& modifier = QString(),
                             const SketchModel* model = nullptr);

    // 块引用（BlockRef）：只创建"引用骨架"（块名 + 插入点/旋转/缩放）。
    // 属性默认值与动态参数默认值需由调用方从块定义补全 —— 常规路径请用
    // SketchModel::insertBlock()（它会调 block::makeRef 一并填好），本方法供
    // 反序列化 / 测试等已持有完整字段的场景使用。
    static Element createBlockRef(const QString& blockName, const Vec3& insertPos,
                                  double rotationDeg, double sx, double sy,
                                  const SketchModel* model);

    // ---- 引导线（预览辅助线，不套样式）----
    static Element createGuideLine(const Vec3& a, const Vec3& b);
    static Element createGuideCircle(const Vec3& center, double radius);

    // 构造线（Xline）：向两端无限延伸的辅助线。through=通过点(世界坐标)，angleDeg=方向角(度)。
    // 复用 Element.pts[0]=through + Element.rotation=angleDeg，无需新增字段。
    static Element createXline(const Vec3& through, double angleDeg, const QString& name, const SketchModel* model);
    static Element createXline(const Vec3& through, double angleDeg, const SketchModel* model);  // 默认名"构造线"
};

} // namespace SketchCAD
