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
    static Element createDimension(DimensionType dt, const SketchModel* model);
    // 文本图元（标题栏栏目、说明文字等）。文字高度(mm) 存于 radius 字段；
    // 颜色/线型/线宽走默认样式，调用方可再按需覆盖。
    static Element createText(const Vec3& pos, const QString& text, double heightMm,
                              const QColor& color = QColor(26, 29, 40));

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
