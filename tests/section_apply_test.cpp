// 剖面填充「写入路径」集成回归（GB/T 4457.5-2013）
// ----------------------------------------------------------------------------
// 背景：section_test.cpp 只覆盖纯核心算法（geo::hatchLines / Section 引擎），
//       完全没走「选中的闭合图元 → Change → applyChanges → SketchModel」这条
//       落盘路径，于是漏掉了一个致命接线缺陷：
//         makeHatchElement() 默认 id=0，若调用方不把源图元 ID 写进
//         Change::replacement.id，applyChanges 会调用 updateElementQuiet(0,...)，
//         因找不到该图元而静默返回 false —— 界面表现即「点确定没有任何反应」。
// 本测试用 SketchModel 实例复现该路径，锁死以下不变量：
//   1) 正常路径：圆被同一个 ID 的 Hatch 图元替换（不是新增、ID 不变）；
//   2) 历史缺陷回归：replacement.id 未赋值（残留 0）时仍须按 Change::id 替换；
//   3) 非闭合图元不产生动作；全非闭合时返回空列表；
//   4) 多区域自动区分（4.7）在集成层生效；关闭开关则方向一致；
//   5) 可一次撤销回原图元；对已是 Hatch 的区域再次填充幂等。
// 需要 QCoreApplication（SketchModel 为 QObject），无需 GUI。
#include "section_test_helpers.h"
#include <QCoreApplication>
#include <QColor>
static Element makeSquare(double x0, double y0, double x1, double y1) {
    Element e; e.type = ElementType::Polygon; e.closed = true;
    e.pts = { Vec3(x0, y0, 0), Vec3(x1, y0, 0), Vec3(x1, y1, 0), Vec3(x0, y1, 0) };
    return e;
}

// 与 MainWindow::sectionFill 同构的落盘动作：beginEdit → applyChanges
static std::vector<int> runSection(SketchModel& model, const std::vector<Element>& srcs,
                                   const SectionStyle& st, bool autoDistinguish,
                                   std::vector<modify::Change>* outChanges = nullptr) {
    const auto changes = makeSectionChanges(srcs, st, autoDistinguish);
    if (outChanges) *outChanges = changes;
    if (changes.empty()) return {};
    std::vector<int> ids;
    for (const auto& c : changes) ids.push_back(c.id);
    model.beginEdit(QStringLiteral("剖面填充"));
    modify::applyChanges(&model, changes, QString());
    return ids;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    printf("=== 剖面填充写入路径集成回归（GB/T 4457.5-2013）===\n");

    // ---- 1. 正常路径：选中圆 → 同 ID 替换为 Hatch 图元 ----
    {
        SketchModel model;
        Element circle = makeCircle(Vec3(0, 0, 0), 5.0);
        circle.color = QColor(255, 0, 0);
        circle.layer = QStringLiteral("L1");
        const int cid = model.addElement(circle);
        model.setSelection({ cid });

        std::vector<Element> srcs;
        for (int id : model.selection())
            if (const Element* e = model.element(id)) srcs.push_back(*e);
        check(srcs.size() == 1, "选择集收集到 1 个源图元");

        SectionStyle st;               // 默认 45° / 3mm / 无豁免 / 通用金属
        st.spacingMm = 1.0;            // 加密以便统计条数
        std::vector<modify::Change> chs;
        const auto ids = runSection(model, srcs, st, true, &chs);

        check(chs.size() == 1, "圆 → 产生 1 个剖面填充动作", QString::number(chs.size()));
        check(chs.size() == 1 && chs[0].id == cid, "Change::id 指向源图元");
        // ▼ 关键不变量：replacement.id 必须等于源 ID（漏写即静默失败）
        check(chs.size() == 1 && chs[0].replacement.id == cid,
              "Change::replacement.id 指向源图元（历史缺陷点）");
        check(chs.size() == 1 && chs[0].replacement.type == ElementType::Hatch,
              "replacement 类型为 Hatch");

        check(model.elements().size() == 1, "图元数不变（替换而非新增）",
              QString::number(model.elements().size()));
        check(ids.size() == 1 && ids[0] == cid, "动作复用了源图元 ID");

        const Element* got = model.element(cid);
        check(got != nullptr, "原 ID 仍可查到图元");
        check(got && got->type == ElementType::Hatch, "原 ID 的图元已变为剖面区域(Hatch)");
        check(got && got->color == QColor(255, 0, 0), "继承源图元颜色");
        check(got && got->layer == QStringLiteral("L1"), "继承源图元图层");
        check(got && got->holes.empty(), "单一圆轮廓：无内孔");

        // 实际能生成剖面线，且全部落在圆内
        if (got && got->type == ElementType::Hatch) {
            const SectionRegion back = hatchRegionFromElement(*got);
            const auto segs = generateHatchLines(back);
            check(segs.size() >= 8, "生成足量剖面线（间距 1mm，半径 5）",
                  QString::number(segs.size()));
            bool allInside = true;
            for (const auto& s : segs) {
                const Vec3 mid((s.a.x + s.b.x) * 0.5, (s.a.y + s.b.y) * 0.5, 0);
                if (mid.distTo(Vec3(0, 0, 0)) > 5.0 + 1e-6) allInside = false;
            }
            check(allInside, "所有剖面线中点都在圆轮廓内");
            check(near2(back.style.angleDeg, 45.0, 1e-9), "剖面线方向 45°（4.1）");
            check(near2(back.style.spacingMm, 1.0, 1e-9), "剖面线间距随参数写入并读回");
        }

        // ---- 一次撤销即可回退 ----
        model.undo();
        const Element* undone = model.element(cid);
        check(undone && undone->type == ElementType::Circle, "撤销一次即回到圆（单次快照）");
        check(undone && near2(undone->radius, 5.0, 1e-9), "撤销后半径还原");
    }

    // ---- 2. 历史缺陷回归：replacement.id 残留 0 时仍须按 Change::id 替换 ----
    {
        SketchModel model;
        const int rid = model.addElement(makeSquare(0, 0, 10, 10));
        SectionStyle st;
        SectionRegion r;
        r.boundary = { Vec3(0, 0, 0), Vec3(10, 0, 0), Vec3(10, 10, 0), Vec3(0, 10, 0) };
        r.style = st;
        Element h = makeHatchElement(r, QStringLiteral("剖面区域"));
        check(h.id == 0, "makeHatchElement 默认 id = 0（缺陷来源）");

        modify::Change c;
        c.id = rid;
        c.replacement = h;             // 故意不设置 replacement.id，保持 0
        check(c.replacement.id == 0, "构造出 replacement.id = 0 的历史缺陷场景");
        model.beginEdit(QStringLiteral("剖面填充"));
        modify::applyChanges(&model, { c }, QString());

        const Element* got = model.element(rid);
        check(got != nullptr && got->type == ElementType::Hatch,
              "replacement.id=0 时，applyChanges 仍以 Change::id 为准完成替换");
        check(model.elements().size() == 1, "未产生游离图元（id=0 未被新增）");
    }

    // ---- 3. 非闭合图元：跳过；全非闭合 → 空动作 ----
    {
        SketchModel model;
        Element line = makeLine(Vec3(0, 0, 0), Vec3(10, 0, 0));
        Element circ = makeCircle(Vec3(20, 20, 0), 4.0);
        const int lid = model.addElement(line);
        const int cid = model.addElement(circ);

        std::vector<Element> srcs;
        for (int id : { lid, cid }) if (const Element* e = model.element(id)) srcs.push_back(*e);
        SectionStyle st;
        auto chs = makeSectionChanges(srcs, st, true);
        check(chs.size() == 1, "直线+圆 → 只对圆产生动作", QString::number(chs.size()));
        check(chs.size() == 1 && chs[0].id == cid, "动作目标是圆而非直线");

        std::vector<Element> onlyLine;
        onlyLine.push_back(*model.element(lid));
        check(makeSectionChanges(onlyLine, st, true).empty(),
              "仅直线 → 空动作（UI 据此提示用户）");
    }

    // ---- 4. 多区域自动区分（GB 4.7 在集成层生效）----
    {
        SketchModel model;
        const int a = model.addElement(makeSquare(0, 0, 10, 10));
        const int b = model.addElement(makeSquare(5, 5, 15, 15));
        std::vector<Element> srcs = { *model.element(a), *model.element(b) };

        SectionStyle st;               // angleDeg = 45
        auto on = makeSectionChanges(srcs, st, true);
        check(on.size() == 2, "两区域 → 2 个动作", QString::number(on.size()));
        check(on.size() == 2 && near2(on[0].replacement.rotation, 45.0, 1e-9) &&
              near2(on[1].replacement.rotation, 135.0, 1e-9),
              "开启自动区分：相邻区域方向相反 45°/135°",
              on.size() == 2 ? QString("%1 / %2").arg(on[0].replacement.rotation)
                                                 .arg(on[1].replacement.rotation)
                             : QString());

        auto off = makeSectionChanges(srcs, st, false);
        check(off.size() == 2 && near2(off[0].replacement.rotation, 45.0, 1e-9) &&
              near2(off[1].replacement.rotation, 45.0, 1e-9),
              "关闭自动区分：两区域方向一致（45°）");
    }

    // ---- 5. 幂等：对已是 Hatch 的区域再次填充 ----
    {
        SketchModel model;
        const int cid = model.addElement(makeCircle(Vec3(0, 0, 0), 6.0));
        SectionStyle st;
        std::vector<Element> srcs = { *model.element(cid) };
        runSection(model, srcs, st, true);

        const Element* once = model.element(cid);
        check(once && once->type == ElementType::Hatch, "首次填充成功");
        std::vector<Element> again = { *once };
        auto chs2 = makeSectionChanges(again, st, true);
        check(chs2.size() == 1 && chs2[0].replacement.id == cid,
              "对已有剖面区域再次执行：仍产出同 ID 动作（幂等不炸）");
        const auto segs = generateHatchLines(hatchRegionFromElement(*model.element(cid)));
        check(!segs.empty(), "重复执行后剖面线仍可生成");
    }

    printf(g_fail == 0 ? "SectionApplyTest ALL PASS\n" : "SectionApplyTest FAILED (%d)\n", g_fail);
    return g_fail == 0 ? 0 : 1;
}
