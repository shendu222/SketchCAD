// 外部参照（Xref）功能单元测试：在核心层 SketchModel 上验证「附加 / 路径类型 /
// 失败分支 / 嵌套参照引入 / 分离（含级联清理）/ 序列化 / 撤销」全链路。
//
// 参照 AutoCAD 的两条行为准则作为可验证推论：
//   1) 附加后外部图形表现为本图一条块表记录 + 任意多个参照实例，符号名 = 文件名主体；
//   2) 分离必须同时抹掉「定义」与「全部实例」；嵌套参照不能直接被分离，只允许随宿主级联消失。
//
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Block.h"
#include "core/Xref.h"
#include "core/CadFormats.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cmath>

using namespace SketchCAD;

static int g_fail = 0;
static void check(bool ok, const char* what, const QString& detail = QString()) {
    if (ok) { printf("  [PASS] %s\n", what); }
    else    { printf("  [FAIL] %s %s\n", what,
                      detail.isEmpty() ? "" : ("(" + detail + ")").toUtf8().constData()); ++g_fail; }
}
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

static Element mkLine(double x1, double y1, double x2, double y2) {
    Element e; e.type = ElementType::Line;
    e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}
static Element mkCircle(double cx, double cy, double r) {
    Element e; e.type = ElementType::Circle; e.pts = {Vec3(cx, cy, 0)}; e.radius = r;
    return e;
}
// 写一个本程序 JSON 图纸（可选带块表），作为「外部参照源文件」
static bool writeJsonDrawing(const QString& path, const std::vector<Element>& elems,
                             const QJsonArray& blocks = QJsonArray()) {
    QJsonObject root;
    root[QStringLiteral("app")] = QStringLiteral("SketchCAD");
    root[QStringLiteral("formatVersion")] = 1;
    root[QStringLiteral("scaleRatio")] = 1.0;
    QJsonArray arr;
    for (const auto& e : elems) arr.append(elementToJson(e));
    root[QStringLiteral("elements")] = arr;
    if (!blocks.isEmpty()) root[QStringLiteral("blocks")] = blocks;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}
static int countRefs(const SketchModel& m, const QString& blockName) {
    int n = 0;
    for (const auto& e : m.elements())
        if (e.type == ElementType::BlockRef && e.blockName == blockName) ++n;
    return n;
}

int main() {
    QTemporaryDir tmp;
    if (!tmp.isValid()) { printf("临时目录创建失败\n"); return 1; }
    const QString dir = tmp.path();

    // ============================================================
    // 1. 附加：默认插入参数 + 引用式展开
    // ============================================================
    printf("\n[1] 附加外部参照\n");
    const QString refPath = dir + QStringLiteral("/ref.json");
    check(writeJsonDrawing(refPath, {mkLine(0, 0, 10, 0), mkCircle(0, 0, 5)}),
          "写出外部参照源文件 ref.json");
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        const int id = m.attachXref(refPath);
        check(id >= 0, "附加外部参照成功（返回参照实例 id）", QString::number(id));
        check(m.hasBlock(QStringLiteral("ref")), "符号名 = 文件名主体");
        const BlockDef* d = m.findBlockDef(QStringLiteral("ref"));
        check(d && d->isXref && !d->xrefNested && d->xrefStatus == (int)XrefStatus::Loaded,
              "定义被标记为已载入的外部参照（非嵌套）");
        check(d && d->xrefPathType == (int)XrefPathType::FullPath &&
                   d->xrefPath == QDir::cleanPath(refPath),
              "路径类型默认=完整路径", d ? d->xrefPath : QString());
        check(d && d->entities.size() == 2, "源图几何融入块定义（2 个图元）",
              d ? QString::number(d->entities.size()) : QString());
        check(countRefs(m, QStringLiteral("ref")) == 1, "附加后本图有 1 个参照实例");
        check(m.xrefNames().size() == 1 && m.xrefNames().front() == QStringLiteral("ref"),
              "xrefNames 列出该参照");
        check(m.hasXrefSource(refPath), "按绝对路径可判定已附加");

        // 默认插入参数：插入点 (0,0) + 缩放 1 + 旋转 0 → 展开即源图原位
        const auto ex = m.expandBlockRef(*m.element(id));
        check(ex.size() == 2, "默认参数下参照可展开");
        check(!ex.empty() && near(ex[0].pts[0].x, 0) && near(ex[0].pts[1].x, 10),
              "默认插入点/比例/旋转 → 展开几何与源图一致",
              QString("%1→%2").arg(ex.empty() ? -999.0 : ex[0].pts[0].x,
                                   ex.empty() ? -999.0 : ex[0].pts[1].x));
        const Element* ref = m.element(id);
        check(ref && near(ref->scaleX, 1.0) && near(ref->scaleY, 1.0) && near(ref->rotation, 0.0),
              "参照实例的缩放/旋转取默认值 1.0 / 0°");
        check(ref && ref->pts.size() == 1 && near(ref->pts[0].x, 0) && near(ref->pts[0].y, 0),
              "参照实例的插入点默认取原点");
    }

    // ============================================================
    // 2. 路径类型：完整路径 / 相对路径 / 无路径
    // ============================================================
    printf("\n[2] 路径类型\n");
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        const int id = m.attachXref(refPath, XrefPathType::RelativePath);
        check(id >= 0, "以「相对路径」附加成功");
        const BlockDef* d = m.findBlockDef(QStringLiteral("ref"));
        check(d && d->xrefPath == QStringLiteral("ref.json"), "记录的相对路径形如 ref.json",
              d ? d->xrefPath : QString());
        check(m.xrefResolvedPath(QStringLiteral("ref")) == QDir::cleanPath(refPath),
              "相对路径可按宿主图目录还原为绝对路径",
              m.xrefResolvedPath(QStringLiteral("ref")));
    }
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        const int id = m.attachXref(refPath, XrefPathType::NoPath);
        check(id >= 0, "以「无路径」附加成功");
        const BlockDef* d = m.findBlockDef(QStringLiteral("ref"));
        check(d && d->xrefPath == QStringLiteral("ref.json"), "「无路径」只记录文件名",
              d ? d->xrefPath : QString());
    }
    {
        // 未存盘的宿主图没有解析基准 → 相对路径退化为纯文件名（不假装有基准）
        SketchModel m;
        check(m.drawingDir().isEmpty(), "未登记宿主图路径时 drawingDir 为空");
        const int id = m.attachXref(refPath, XrefPathType::RelativePath);
        const BlockDef* d = m.findBlockDef(QStringLiteral("ref"));
        check(id >= 0 && d && d->xrefPath == QStringLiteral("ref.json"),
              "无宿主目录时相对路径退化为文件名");
    }

    // ============================================================
    // 3. 失败分支（均返回 -1 且不产生副作用）
    // ============================================================
    printf("\n[3] 失败分支\n");
    {
        SketchModel m;
        check(m.attachXref(QString()) < 0, "空路径被拒绝");
        check(m.attachXref(dir + QStringLiteral("/不存在.json")) < 0, "文件不存在被拒绝");
        check(m.blockCount() == 0 && m.elementCount() == 0, "失败不留下任何定义/图元");
    }
    {
        // STEP 只写不读，不能作为参照源
        SketchModel m;
        check(m.attachXref(refPath) >= 0, "首次附加成功");
        check(m.attachXref(refPath) < 0, "重复附加同一文件被拒绝");
        check(m.xrefCount() == 1 && countRefs(m, QStringLiteral("ref")) == 1,
              "重复附加后定义与实例数量不变");
    }
    {
        const QString badPath = dir + QStringLiteral("/bad.txt");
        QFile f(badPath); (void)f.open(QIODevice::WriteOnly);
        f.write("hello, not a drawing"); f.close();
        SketchModel m;
        check(m.attachXref(badPath) < 0, "不支持的文件格式被拒绝");
        check(m.blockCount() == 0, "格式失败后不留下定义");
    }
    {
        // 同名普通块占用：不能把普通块当成参照来覆盖/分离
        SketchModel m;
        BlockDef plain; plain.name = QStringLiteral("ref");
        plain.entities = { mkLine(0, 0, 1, 0) };
        check(m.addBlockDef(plain), "先建立同名普通块 ref");
        check(m.attachXref(refPath) < 0, "块名被普通块占用时附加被拒绝");
        check(!m.findBlockDef(QStringLiteral("ref"))->isXref, "原普通块未被改成外部参照");
        check(!m.detachXref(QStringLiteral("ref")), "对普通块执行分离被拒绝");
        check(m.hasBlock(QStringLiteral("ref")), "分离失败后普通块仍然存在");
    }

    // ============================================================
    // 4. DXF / STEP 作为参照源
    // ============================================================
    printf("\n[4] DXF/DWG 作为参照源\n");
    {
        const QString dxfPath = dir + QStringLiteral("/part.dxf");
        QString err;
        const QByteArray dxf = cadfmt::toDxf({mkLine(0, 0, 20, 0)}, &err);
        check(!dxf.isEmpty(), "生成 DXF 参照源", err);
        QFile f(dxfPath); (void)f.open(QIODevice::WriteOnly); f.write(dxf); f.close();

        SketchModel m;
        const int id = m.attachXref(dxfPath);
        check(id >= 0, "附加 DXF 作为外部参照成功");
        const BlockDef* d = m.findBlockDef(QStringLiteral("part"));
        check(d && d->isXref && d->entities.size() == 1, "DXF 几何读入参照定义",
              d ? QString::number(d->entities.size()) : QString());

        // STEP 为只写格式，不能作为参照读入
        const QString stepPath = dir + QStringLiteral("/part.step");
        QFile sf(stepPath); (void)sf.open(QIODevice::WriteOnly); sf.write("ISO-10303-21;"); sf.close();
        SketchModel m2;
        check(m2.attachXref(stepPath) < 0, "STEP 只写格式：作为参照读入被拒绝");
    }

    // ============================================================
    // 5. 嵌套参照：依赖引入 + 不可直接分离 + 级联清理
    // ============================================================
    printf("\n[5] 嵌套参照\n");
    {
        // 构造一个「自带块定义且在顶层引用它」的源文件
        SketchModel src;
        const int l = src.addElement(mkLine(0, 0, 10, 0));
        src.setSelection({l});
        src.createBlockFromSelection(QStringLiteral("子件"), Vec3(0, 0, 0), false);
        Element childRef = block::makeRef(*src.findBlockDef(QStringLiteral("子件")),
                                          Vec3(50, 50, 0), 0.0, 1.0, 1.0);
        const QString nestedPath = dir + QStringLiteral("/nested.json");
        check(writeJsonDrawing(nestedPath, {childRef}, src.blocksToJson()),
              "写出含嵌套块引用的参照源文件");

        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        const int id = m.attachXref(nestedPath);
        check(id >= 0, "附加带依赖的参照成功");
        check(m.hasBlock(QStringLiteral("子件")), "被引用的依赖被引入本图");
        const BlockDef* dep = m.findBlockDef(QStringLiteral("子件"));
        check(dep && dep->xrefNested, "依赖标记为嵌套参照");
        check(m.xrefNames().size() == 1 && m.xrefNames().front() == QStringLiteral("nested"),
              "xrefNames（默认）只列直接附加的参照");
        check(m.xrefNames(true).size() == 2, "xrefNames(含嵌套) 列出 2 条");
        // 嵌套不被支持直接分离
        check(!m.detachXref(QStringLiteral("子件")), "嵌套参照不允许在本图直接分离");
        check(m.hasBlock(QStringLiteral("子件")), "分离被拒后嵌套参照仍在");

        // 宿主被分离 → 失去全部引用者的嵌套参照级联消失
        check(m.detachXref(QStringLiteral("nested")), "分离宿主参照成功");
        check(!m.hasBlock(QStringLiteral("nested")), "宿主定义已移除");
        check(!m.hasBlock(QStringLiteral("子件")), "无引用者的嵌套参照被级联清理");
        check(countRefs(m, QStringLiteral("nested")) == 0, "宿主的全部实例已移除");
    }

    // ============================================================
    // 6. 分离：定义 + 全部实例
    // ============================================================
    printf("\n[6] 分离\n");
    {
        SketchModel m;
        const int id1 = m.attachXref(refPath);
        const int nStale = m.insertBlock(QStringLiteral("ref"), Vec3(100, 0, 0));   // 第 2 个实例
        (void)nStale;
        const int keep = m.addElement(mkLine(0, 0, 1, 0));                          // 无关图元
        check(countRefs(m, QStringLiteral("ref")) == 2, "图内共 2 个参照实例");

        check(!m.detachXref(QStringLiteral("不存在")), "块名不存在时分离被拒绝");
        check(m.detachXref(QStringLiteral("ref")), "分离成功");
        check(!m.hasBlock(QStringLiteral("ref")), "参照定义被移除");
        check(countRefs(m, QStringLiteral("ref")) == 0, "全部参照实例被移除");
        check(m.element(keep) != nullptr, "无关图元未受影响");
        check(m.xrefCount() == 0, "分离后没有外部参照");

        // 一次 Ctrl+Z 全量回滚：定义 + 两个实例
        m.undo();
        check(m.hasBlock(QStringLiteral("ref")), "撤销分离：定义恢复");
        check(countRefs(m, QStringLiteral("ref")) == 2, "撤销分离：2 个实例同时恢复");
        (void)id1;
    }

    // ============================================================
    // 7. 附加的撤销
    // ============================================================
    printf("\n[7] 附加的撤销 / 重做\n");
    {
        SketchModel m;
        const int id = m.attachXref(refPath);
        check(id >= 0 && m.xrefCount() == 1, "附加成功");
        m.undo();
        check(m.xrefCount() == 0 && m.elementCount() == 0, "撤销附加：定义与实例一并回退");
        check(countRefs(m, QStringLiteral("ref")) == 0, "撤销附加：实例清空");
        m.redo();
        check(m.xrefCount() == 1 && countRefs(m, QStringLiteral("ref")) == 1,
              "重做附加：定义与实例恢复");
    }

    // ============================================================
    // 8. 序列化（随图存盘 / 旧档兼容）
    // ============================================================
    printf("\n[8] 序列化\n");
    {
        SketchModel m;
        m.setDrawingPath(dir + QStringLiteral("/host.json"));
        const int id = m.attachXref(refPath, XrefPathType::RelativePath);
        check(id >= 0, "附加用于序列化检查");
        const Element before = *m.element(id);

        const QJsonArray arr = m.blocksToJson();
        SketchModel m2;
        m2.setBlocksFromJson(arr);
        check(m2.xrefCount() == 1, "外部参照随块表序列化往返");
        const BlockDef* g = m2.findBlockDef(QStringLiteral("ref"));
        check(g && g->isXref && g->xrefPath == QStringLiteral("ref.json") &&
                   g->xrefPathType == (int)XrefPathType::RelativePath && !g->xrefNested,
              "读回：路径 / 路径类型 / 嵌套标记保持");

        // 图元（参照实例）往返后仍可展开
        const Element carried = elementFromJson(elementToJson(before));
        const auto ex = m2.expandBlockRef(carried);
        check(ex.size() == 2, "读回后参照实例仍可展开为源图几何");

        // 普通块不带 xref 键 —— 与引入 Xref 之前完全一致的存档内容
        SketchModel m3;
        BlockDef plain; plain.name = QStringLiteral("普通");
        plain.entities = { mkLine(0, 0, 1, 0) };
        m3.addBlockDef(plain);
        const QJsonObject o = m3.blocksToJson().at(0).toObject();
        check(!o.contains(QStringLiteral("xref")), "普通块不写入 xref 字段（旧档格式不变）");
        SketchModel m4;
        m4.setBlocksFromJson(m3.blocksToJson());
        const BlockDef* p = m4.findBlockDef(QStringLiteral("普通"));
        check(p && !p->isXref, "旧存档（无 xref 键）读回为普通块");
    }

    printf("\n外部参照测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
