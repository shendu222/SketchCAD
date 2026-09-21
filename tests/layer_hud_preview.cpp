// 一次性预览：把「当前图层」HUD 徽标 + 图层继承效果渲染成 PNG，
// 用于在真实 GUI 之外人工核对视觉表现（不改变任何被测逻辑）。
#include <QApplication>
#include <QImage>
#include <QPainter>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/Element.h"
#include "core/ElementFactory.h"
#include "core/Geometry.h"
#include <cstdio>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace SketchCAD {

class TestView : public GraphicsView {
public:
    explicit TestView(SketchModel* m, QWidget* p = nullptr) : GraphicsView(m, p) {}
};

} // namespace SketchCAD

using namespace SketchCAD;

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    SketchModel model;
    // 预置标准图层模板，并新建一个带颜色的自定义图层
    for (const Layer& l : standardLayers()) model.addLayer(l.name, l.color, l.lineWidth, l.lineStyle);
    model.addLayer(QStringLiteral("尺寸线"), QColor(220, 40, 40), 2.0, LineStyle::Dashed);
    model.setCurrentLayer(QStringLiteral("尺寸线"));   // 切换到自定义层 → 新建图元继承其属性

    TestView view(&model);
    view.resize(960, 600);
    model.setShowGrid(true);

    // 用工厂创建（等效于用户交互绘制）：自动归属当前层并继承红色/虚线/2.0
    model.addElement(ElementFactory::createLine(Vec3(-120,  40, 0), Vec3(120,  40, 0), &model));
    model.addElement(ElementFactory::createLine(Vec3(-120, -40, 0), Vec3(120, -40, 0), &model));
    model.addElement(ElementFactory::createCircle(Vec3(0, 0, 0), 55.0, &model));
    view.fitView();

    QImage img(960, 600, QImage::Format_ARGB32);
    img.fill(QColor(245, 247, 250));
    {
        QPainter p(&img);
        view.render(&p);
    }
    const bool ok = img.save(QStringLiteral("build/layer_hud_preview.png"));
    printf("layer_hud_preview saved: %s\n", ok ? "YES" : "NO");
    return ok ? 0 : 1;
}
