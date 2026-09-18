// 飞机三面图预览：offscreen 生成并渲染为 PNG，供人工核对图形布局；
// 同时把同一份图元写出 DXF / STEP 样例，便于用 AutoCAD / 看图软件核对交换格式结果。
#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QFile>
#include "ui/GraphicsView.h"
#include "core/SketchModel.h"
#include "core/AircraftViews.h"
#include "core/Geometry.h"
#include "core/CadFormats.h"

using namespace SketchCAD;

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    SketchModel model;
    GraphicsView view(&model);
    view.resize(1200, 900);
    view.show();

    aircraft::AircraftViewsParams p;
    const auto elems = aircraft::buildThreeViews(p, &model);
    model.addElementsQuiet(elems);

    // 按图元包围盒居中缩放，避免被巨大网格背景冲淡
    QRectF bbox;
    for (const auto& e : elems) bbox |= geo::elementBounds(e);
    if (bbox.width() > 1e-6 && bbox.height() > 1e-6) {
        bbox.adjust(-bbox.width() * 0.08, -bbox.height() * 0.08,
                     bbox.width() * 0.08,  bbox.height() * 0.08);
        view.fitInView(bbox, Qt::KeepAspectRatio);
    }
    QApplication::processEvents();

    QImage img(view.size(), QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter painter(&img);
    view.render(&painter);
    painter.end();

    const QString path = QStringLiteral("build/aircraft_preview.png");
    if (!img.save(path)) {
        qWarning("无法保存预览图到 %s", qPrintable(path));
        return 1;
    }
    qInfo("已保存飞机三面图预览: %s (%dx%d)", qPrintable(path), img.width(), img.height());

    // ---- 交换格式样例：与程序「保存」走同一套核心导出函数 ----
    // body 用 QByteArray：DXF 的字节就是最终字节（含中文时为 GBK），不能再 toUtf8()
    const auto dump = [](const QString& p, const QByteArray& body, const char* what) {
        QFile f(p);
        if (f.open(QIODevice::WriteOnly) && f.write(body) > 0)
            qInfo("已导出 %s: %s (%lld 字节)", what, qPrintable(p), (long long)body.size());
        else
            qWarning("无法写出 %s 到 %s", what, qPrintable(p));
    };
    const std::vector<Element>& all = model.elements();
    dump(QStringLiteral("build/aircraft_preview.dxf"),
         cadfmt::toDxf(all, nullptr), "DXF");
    cadfmt::StepStats st;
    dump(QStringLiteral("build/aircraft_preview.step"),
         cadfmt::toStep(all, &st, nullptr).toUtf8(), "STEP");
    qInfo("STEP 曲线实体数 = %d，未导出文本 = %d", st.curves, st.skippedText);
    return 0;
}
