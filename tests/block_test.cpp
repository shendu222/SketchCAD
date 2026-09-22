// 块（Block）功能单元测试：在核心层 SketchModel 上验证「块定义 / 插入实例化 /
// 重定义同步 / 动态块 / 属性块 / 嵌套 / 序列化 / 撤销」全链路。
//
// 采用「引用式」模型的三个可验证推论：
//   1) 块内几何不复制 —— 展开结果由 BlockDef 实时计算；
//   2) 重定义只改一份定义 —— 所有引用同步更新（无需反向推送）；
//   3) 展开是世界坐标 —— 插入平移/旋转/缩放后，子图元几何按仿射变换可精确断言。
//
// 运行平台：纯核心层，无需 GUI（QT_QPA_PLATFORM 任意）。
#include "core/SketchModel.h"
#include "core/ElementFactory.h"
#include "core/Block.h"
#include "core/Geometry.h"
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
static Element mkRect(double x1, double y1, double x2, double y2) {
    Element e; e.type = ElementType::Rectangle; e.pts = {Vec3(x1, y1, 0), Vec3(x2, y2, 0)};
    return e;
}
static int countOfType(const std::vector<Element>& v, ElementType t) {
    int n = 0; for (const auto& e : v) if (e.type == t) ++n; return n;
}

int main() {
    // ============================================================
    // 1. 块定义与创建流程（从选择集建块）
    // ============================================================
    printf("\n[1] 块定义与创建流程\n");
    {
        SketchModel m;
        const int l1 = m.addElement(mkLine(0, 0, 10, 0));
        const int l2 = m.addElement(mkLine(10, 0, 10, 5));
        m.setSelection({l1, l2});
        const int refId = m.createBlockFromSelection(QStringLiteral("L支架"), Vec3(0, 0, 0), true);
        check(m.hasBlock(QStringLiteral("L支架")), "从选择集创建块定义成功");
        check(m.findBlockDef(QStringLiteral("L支架"))->entities.size() == 2, "块定义含 2 个块内图元");
        check(m.elementCount() == 1 && m.element(refId) &&
              m.element(refId)->type == ElementType::BlockRef, "delete 模式：原图元被 1 个块引用替换");
        check(m.element(refId)->blockName == QStringLiteral("L支架"), "块引用指向正确的块名");
        // 展开应还原原几何（插入点 = 基点 = 原点，恒等变换）
        const auto ex = m.expandBlockRef(*m.element(refId));
        check(ex.size() == 2 && countOfType(ex, ElementType::Line) == 2, "展开得到 2 条直线");
        check(mkLine(0, 0, 10, 0).pts[0].distTo(ex[0].pts[0]) < 1e-9 &&
              mkLine(0, 0, 10, 0).pts[1].distTo(ex[0].pts[1]) < 1e-9,
              "基点=插入点=原点时展开为恒等变换");

        // keep 模式：原图元保留、不放置引用
        SketchModel m2;
        const int a = m2.addElement(mkLine(0, 0, 4, 0));
        m2.setSelection({a});
        const int r2 = m2.createBlockFromSelection(QStringLiteral("K"), Vec3(0, 0, 0), false);
        check(r2 == -1 && m2.hasBlock(QStringLiteral("K")) && m2.elementCount() == 1,
              "keep 模式：创建块定义但保留原图元、不放置引用");

        // 重名 / 空名拒绝
        check(!m.addBlockDef(BlockDef{}), "空块名拒绝创建");
        BlockDef dup; dup.name = QStringLiteral("L支架");
        check(!m.addBlockDef(dup), "重名块拒绝创建");
    }

    // ============================================================
    // 2. 插入与实例化（平移 / 旋转 / 缩放）
    // ============================================================
    printf("\n[2] 插入与实例化\n");
    {
        SketchModel m;
        const int l = m.addElement(mkLine(0, 0, 10, 0));
        m.setSelection({l});
        m.createBlockFromSelection(QStringLiteral("B"), Vec3(0, 0, 0), true);

        const int refA = m.insertBlock(QStringLiteral("B"), Vec3(100, 50, 0));
        const auto exA = m.expandBlockRef(*m.element(refA));
        check(near(exA[0].pts[0].x, 100) && near(exA[0].pts[0].y, 50) &&
              near(exA[0].pts[1].x, 110) && near(exA[0].pts[1].y, 50),
              "平移插入：几何整体偏移 (100,50)",
              QString("%1,%2").arg(exA[0].pts[0].x).arg(exA[0].pts[0].y));

        const int refR = m.insertBlock(QStringLiteral("B"), Vec3(0, 0, 0), 90.0);
        const auto exR = m.expandBlockRef(*m.element(refR));
        check(near(exR[0].pts[0].x, 0) && near(exR[0].pts[0].y, 0) &&
              near(exR[0].pts[1].x, 0) && near(exR[0].pts[1].y, 10),
              "旋转插入 90°：(10,0) 映射到 (0,10)",
              QString("%1,%2").arg(exR[0].pts[1].x).arg(exR[0].pts[1].y));

        const int refS = m.insertBlock(QStringLiteral("B"), Vec3(0, 0, 0), 0.0, 2.0, 2.0);
        const auto exS = m.expandBlockRef(*m.element(refS));
        check(near(exS[0].pts[1].x, 20), "等比例缩放 2×：长度 10→20");

        // 非等比例：圆 → 椭圆；带旋转矩形 → 多边形
        SketchModel m3;
        const int c = m3.addElement(mkCircle(0, 0, 10));
        m3.setSelection({c});
        m3.createBlockFromSelection(QStringLiteral("C"), Vec3(0, 0, 0), true);
        const int rc = m3.insertBlock(QStringLiteral("C"), Vec3(0, 0, 0), 0.0, 2.0, 1.0);
        const auto exc = m3.expandBlockRef(*m3.element(rc));
        check(countOfType(exc, ElementType::Ellipse) == 1 &&
              near(exc[0].radius, 20) && near(exc[0].radiusY, 10),
              "非等比例缩放：圆按各轴压成椭圆 (20,10)");

        SketchModel m4;
        const int rr = m4.addElement(mkRect(0, 0, 10, 5));
        m4.setSelection({rr});
        m4.createBlockFromSelection(QStringLiteral("R"), Vec3(0, 0, 0), true);
        const int rrId = m4.insertBlock(QStringLiteral("R"), Vec3(0, 0, 0), 45.0);
        const auto exr = m4.expandBlockRef(*m4.element(rrId));
        check(countOfType(exr, ElementType::Polygon) == 1 && exr[0].pts.size() == 4,
              "带旋转插入：矩形离散为 4 顶点多边形（避免轴对齐表示画错）");

        // 未定义块引用：安全返回空，不崩溃
        Element ghost; ghost.type = ElementType::BlockRef; ghost.blockName = QStringLiteral("不存在");
        ghost.pts = {Vec3(0, 0, 0)};
        check(m.expandBlockRef(ghost).empty(), "未定义块的引用展开为空（不崩溃）");
    }

    // ============================================================
    // 3. 编辑与重定义（块内图元修改 → 所有引用同步）
    // ============================================================
    printf("\n[3] 重定义同步\n");
    {
        SketchModel m;
        const int l = m.addElement(mkLine(0, 0, 10, 0));
        m.setSelection({l});
        m.createBlockFromSelection(QStringLiteral("B"), Vec3(0, 0, 0), true);
        const int r1 = m.insertBlock(QStringLiteral("B"), Vec3(0, 0, 0));
        const int r2 = m.insertBlock(QStringLiteral("B"), Vec3(100, 0, 0));
        check(m.expandBlockRef(*m.element(r1))[0].pts[1].x == 10 &&
              m.expandBlockRef(*m.element(r2))[0].pts[1].x == 110,
              "重定义前：两个引用分别为 (0,0)-(10,0) 与 (100,0)-(110,0)");

        // 用新几何（长度 30 的直线）重定义块
        const int nl = m.addElement(mkLine(0, 0, 30, 0));
        m.setSelection({nl});
        check(m.redefineBlockFromSelection(QStringLiteral("B")), "重定义块成功");
        const auto e1 = m.expandBlockRef(*m.element(r1));
        const auto e2 = m.expandBlockRef(*m.element(r2));
        check(near(e1[0].pts[1].x, 30), "重定义后：引用 1 同步为新几何（长度 30）",
              QString::number(e1[0].pts[1].x));
        check(near(e2[0].pts[1].x, 130), "重定义后：引用 2 同步为新几何（100+30）",
              QString::number(e2[0].pts[1].x));

        // 分解：块引用 → 真实图元
        const int nBefore = m.elementCount();
        auto ids = m.explodeBlockRef(r2);
        check(ids.size() == 1 && m.elementCount() == nBefore, "分解：1 个引用替换为 1 个真实图元");
        check(m.element(ids[0])->type == ElementType::Line, "分解产物的类型为直线（非块引用）");

        // 重命名块：引用同步改名
        check(m.renameBlock(QStringLiteral("B"), QStringLiteral("B2")), "重命名块成功");
        check(m.element(r1)->blockName == QStringLiteral("B2"), "重命名：引用 blockName 同步更新");

        // 删除块：定义消失，引用展开为空（可撤销恢复）
        check(m.removeBlock(QStringLiteral("B2")), "删除块成功");
        check(!m.hasBlock(QStringLiteral("B2")), "删除后块定义不存在");
        check(m.expandBlockRef(*m.element(r1)).empty(), "块定义缺失时引用展开为空");
        m.undo();
        check(m.hasBlock(QStringLiteral("B2")), "撤销删除：块定义恢复");
        check(!m.expandBlockRef(*m.element(r1)).empty(), "撤销删除：引用重新可展开");
    }

    // ============================================================
    // 4. 动态块：可见性 / 拉伸 / 旋转 / 翻转
    // ============================================================
    printf("\n[4] 动态块\n");
    {
        SketchModel m;
        BlockDef d;
        d.name = QStringLiteral("Dyn");
        d.basePoint = Vec3(0, 0, 0);
        d.entities = { mkLine(0, 0, 10, 0), mkLine(0, 5, 10, 5) };
        // 可见性：两个状态
        DynParam vis; vis.name = QStringLiteral("可见性"); vis.type = DynParamType::Visibility;
        vis.states = { VisibilityState{QStringLiteral("全部"), {}},
                       VisibilityState{QStringLiteral("仅下边"), {1}} };
        d.dynParams.push_back(vis);
        // 线性拉伸：影响图元 0，沿 +X 从 (0,0) 起
        DynParam st; st.name = QStringLiteral("宽度"); st.type = DynParamType::LinearStretch;
        st.affected = {0}; st.base = Vec3(0, 0, 0); st.direction = Vec3(1, 0, 0);
        d.dynParams.push_back(st);
        // 旋转：影响图元 0，绕原点
        DynParam rot; rot.name = QStringLiteral("角度"); rot.type = DynParamType::Rotate;
        rot.affected = {0}; rot.base = Vec3(0, 0, 0);
        d.dynParams.push_back(rot);
        // 翻转：影响图元 0，关于 Y 轴（方向 (0,1)）
        DynParam fl; fl.name = QStringLiteral("翻转"); fl.type = DynParamType::Flip;
        fl.affected = {0}; fl.base = Vec3(0, 0, 0); fl.direction = Vec3(0, 1, 0);
        d.dynParams.push_back(fl);
        check(m.addBlockDef(d), "添加动态块定义");
        const int refId = m.insertBlock(QStringLiteral("Dyn"), Vec3(0, 0, 0));
        check(m.expandBlockRef(*m.element(refId)).size() == 2,
              "默认可见性状态：展开 2 条线（全部可见）");

        // 可见性切换
        check(m.setBlockDynState(refId, QStringLiteral("可见性"), QStringLiteral("仅下边")),
              "切换到可见性状态「仅下边」");
        check(m.expandBlockRef(*m.element(refId)).size() == 1,
              "状态「仅下边」：隐藏图元 1，展开 1 条线");

        // 线性拉伸：默认 0 → 设 5
        check(m.setBlockDynValue(refId, QStringLiteral("宽度"), 5.0), "设置拉伸距离 5");
        {
            const auto ex = m.expandBlockRef(*m.element(refId));
            // 图元 0 的可动侧顶点 (10,0) 沿 +X 移动 5 → (15,0)；不动侧 (0,0) 保持
            check(near(ex[0].pts[0].x, 0) && near(ex[0].pts[1].x, 15),
                  "线性拉伸：可动侧顶点 10→15，固定侧不动",
                  QString("%1→%2").arg(ex[0].pts[0].x).arg(ex[0].pts[1].x));
        }
        m.setBlockDynValue(refId, QStringLiteral("宽度"), 0.0);   // 复位

        // 旋转
        m.setBlockDynValue(refId, QStringLiteral("角度"), 90.0);
        check(m.setBlockDynState(refId, QStringLiteral("可见性"), QStringLiteral("全部")),
              "恢复可见性「全部」");
        {
            const auto ex = m.expandBlockRef(*m.element(refId));
            check(near(ex[0].pts[1].x, 0) && near(ex[0].pts[1].y, 10),
                  "旋转 90°：(10,0)→(0,10)",
                  QString("%1,%2").arg(ex[0].pts[1].x).arg(ex[0].pts[1].y));
        }
        m.setBlockDynValue(refId, QStringLiteral("角度"), 0.0);

        // 翻转（关于 Y 轴镜像 → x 取反）
        m.setBlockDynValue(refId, QStringLiteral("翻转"), 1.0);
        {
            const auto ex = m.expandBlockRef(*m.element(refId));
            check(near(ex[0].pts[0].x, 0) && near(ex[0].pts[1].x, -10),
                  "翻转：关于 Y 轴镜像，x 取反 → (-10,0)",
                  QString::number(ex[0].pts[1].x));
        }

        // 值域钳制
        SketchModel m5;
        BlockDef dc;
        dc.name = QStringLiteral("Clamp");
        dc.entities = { mkLine(0, 0, 10, 0) };
        DynParam sc; sc.name = QStringLiteral("W"); sc.type = DynParamType::LinearStretch;
        sc.affected = {0}; sc.base = Vec3(0, 0, 0); sc.direction = Vec3(1, 0, 0);
        sc.minValue = 0.0; sc.maxValue = 4.0;
        dc.dynParams.push_back(sc);
        m5.addBlockDef(dc);
        const int rc2 = m5.insertBlock(QStringLiteral("Clamp"), Vec3(0, 0, 0));
        m5.setBlockDynValue(rc2, QStringLiteral("W"), 999.0);   // 超上限 → 钳到 4
        check(near(m5.expandBlockRef(*m5.element(rc2))[0].pts[1].x, 14),
              "动态参数值域钳制：999 被钳到 4 → 10+4=14");
        check(!m5.setBlockDynState(rc2, QStringLiteral("W"), QStringLiteral("不存在")),
              "对非可见性参数设置状态被拒绝");
    }

    // ============================================================
    // 5. 属性块：定义 / 赋值 / 提取 / 编辑
    // ============================================================
    printf("\n[5] 属性块\n");
    {
        SketchModel m;
        BlockDef d;
        d.name = QStringLiteral("Attr");
        d.basePoint = Vec3(0, 0, 0);
        d.entities = { mkLine(0, 0, 20, 0) };
        AttributeDef a1; a1.tag = QStringLiteral("NAME"); a1.prompt = QStringLiteral("名称");
        a1.defaultValue = QStringLiteral("M8"); a1.pos = Vec3(2, 2, 0); a1.height = 3.0;
        AttributeDef a2; a2.tag = QStringLiteral("QTY"); a2.prompt = QStringLiteral("数量");
        a2.defaultValue = QStringLiteral("1"); a2.pos = Vec3(2, -6, 0);
        d.attdefs = { a1, a2 };
        m.addBlockDef(d);
        const int refId = m.insertBlock(QStringLiteral("Attr"), Vec3(0, 0, 0));
        check(m.element(refId)->blockAttrs.size() == 2, "插入时按属性定义初始化 2 个属性值");
        check(m.element(refId)->blockAttr(QStringLiteral("NAME")) == QStringLiteral("M8"),
              "属性默认值取自定义（NAME=M8）");
        check(countOfType(m.expandBlockRef(*m.element(refId)), ElementType::Text) == 2,
              "展开生成 2 段属性文字");

        // 属性编辑
        check(m.setBlockAttribute(refId, QStringLiteral("NAME"), QStringLiteral("M10")), "编辑属性 NAME=M10");
        {
            const auto ex = m.expandBlockRef(*m.element(refId));
            bool found = false;
            for (const auto& e : ex) if (e.type == ElementType::Text && e.text == QStringLiteral("M10")) found = true;
            check(found, "展开后的属性文字同步为 M10");
        }

        // 属性提取
        const auto ex2 = m.extractBlockAttributes(refId);
        check(ex2.size() == 2, "属性提取返回 2 条");
        check(ex2[0].tag == QStringLiteral("NAME") && ex2[0].value == QStringLiteral("M10"),
              "提取结果含编辑后的值");
        check(m.extractBlockAttributes(9999).empty(), "非块引用/不存在 id 提取为空");

        // 不可见属性：不生成可见文字，但仍参与提取
        BlockDef d2 = d; d2.name = QStringLiteral("Invis");
        d2.attdefs[1].invisible = true;
        m.addBlockDef(d2);
        const int ri = m.insertBlock(QStringLiteral("Invis"), Vec3(0, 0, 0));
        check(countOfType(m.expandBlockRef(*m.element(ri)), ElementType::Text) == 1,
              "不可见属性不生成可见文字");
        check(m.extractBlockAttributes(ri).size() == 2, "不可见属性仍参与属性提取");

        // 常量属性：拒绝编辑
        BlockDef d3 = d; d3.name = QStringLiteral("Const");
        d3.attdefs[0].constant = true;
        m.addBlockDef(d3);
        const int rk = m.insertBlock(QStringLiteral("Const"), Vec3(0, 0, 0));
        check(!m.setBlockAttribute(rk, QStringLiteral("NAME"), QStringLiteral("X")),
              "常量属性拒绝修改");
        // 新增（定义中未声明的）属性 tag 会被追加
        check(m.setBlockAttribute(rk, QStringLiteral("NEW"), QStringLiteral("v")),
              "未声明的属性 tag 可追加");
        check(m.element(rk)->blockAttr(QStringLiteral("NEW")) == QStringLiteral("v"),
              "追加属性值生效");
    }

    // ============================================================
    // 6. 嵌套块与环检测
    // ============================================================
    printf("\n[6] 嵌套块与环检测\n");
    {
        SketchModel m;
        // 子块：一条 (0,0)-(10,0) 的线
        BlockDef child;
        child.name = QStringLiteral("子");
        child.basePoint = Vec3(0, 0, 0);
        child.entities = { mkLine(0, 0, 10, 0) };
        m.addBlockDef(child);

        // 父块：内含一个指向子块的引用，插入点为 (5,5)
        BlockDef parent;
        parent.name = QStringLiteral("父");
        parent.basePoint = Vec3(0, 0, 0);
        Element childRef = block::makeRef(child, Vec3(5, 5, 0), 0.0, 1.0, 1.0);
        parent.entities = { childRef };
        m.addBlockDef(parent);

        const int rp = m.insertBlock(QStringLiteral("父"), Vec3(0, 0, 0));
        const auto ex = m.expandBlockRef(*m.element(rp));
        check(countOfType(ex, ElementType::Line) == 1, "嵌套块展开：父块内子块被递归展开为 1 条线");
        check(near(ex[0].pts[0].x, 5) && near(ex[0].pts[0].y, 5) &&
              near(ex[0].pts[1].x, 15) && near(ex[0].pts[1].y, 5),
              "嵌套变换复合：子块线 (0,0)-(10,0) → (5,5)-(15,5)",
              QString("(%1,%2)-(%3,%4)").arg(ex[0].pts[0].x).arg(ex[0].pts[0].y)
                     .arg(ex[0].pts[1].x).arg(ex[0].pts[1].y));

        // 父块整体平移，嵌套几何随之平移
        const int rp2 = m.insertBlock(QStringLiteral("父"), Vec3(100, 0, 0));
        const auto ex2 = m.expandBlockRef(*m.element(rp2));
        check(near(ex2[0].pts[0].x, 105), "嵌套 + 父块平移：基点 5 → 105",
              QString::number(ex2[0].pts[0].x));

        // 自引用环：安全返回（不递归爆栈）
        BlockDef cyc;
        cyc.name = QStringLiteral("环");
        cyc.basePoint = Vec3(0, 0, 0);
        Element self; self.type = ElementType::BlockRef; self.blockName = QStringLiteral("环");
        self.pts = {Vec3(0, 0, 0)};
        cyc.entities = { self };
        m.addBlockDef(cyc);
        const int rc = m.insertBlock(QStringLiteral("环"), Vec3(0, 0, 0));
        check(m.expandBlockRef(*m.element(rc)).empty(), "自引用块：环检测生效，展开为空且不崩溃");
    }

    // ============================================================
    // 7. 序列化（块表随图存盘 / 图元字段往返）
    // ============================================================
    printf("\n[7] 序列化\n");
    {
        SketchModel m;
        BlockDef d;
        d.name = QStringLiteral("Ser");
        d.basePoint = Vec3(1, 2, 0);
        d.entities = { mkLine(0, 0, 10, 0) };
        AttributeDef a; a.tag = QStringLiteral("T"); a.defaultValue = QStringLiteral("v");
        a.pos = Vec3(1, 1, 0);
        d.attdefs = { a };
        DynParam p; p.name = QStringLiteral("V"); p.type = DynParamType::Visibility;
        p.states = { VisibilityState{QStringLiteral("A"), {}}, VisibilityState{QStringLiteral("B"), {0}} };
        d.dynParams = { p };
        m.addBlockDef(d);
        const int rid = m.insertBlock(QStringLiteral("Ser"), Vec3(3, 4, 0), 30.0, 1.5, 2.0);
        m.setBlockAttribute(rid, QStringLiteral("T"), QStringLiteral("hello"));
        m.setBlockDynState(rid, QStringLiteral("V"), QStringLiteral("B"));

        const QJsonArray arr = m.blocksToJson();
        SketchModel m2;
        m2.setBlocksFromJson(arr);
        check(m2.blockCount() == 1, "块表序列化往返：数量一致");
        const BlockDef* g = m2.findBlockDef(QStringLiteral("Ser"));
        check(g && g->entities.size() == 1 && g->attdefs.size() == 1 && g->dynParams.size() == 1,
              "读回：块内图元 / 属性定义 / 动态参数齐备");
        check(g && g->dynParams[0].states.size() == 2 &&
              g->dynParams[0].states[1].hiddenEntities.size() == 1,
              "读回：可见性状态与隐藏索引完好");
        check(g && near(g->basePoint.x, 1) && near(g->basePoint.y, 2), "读回：基点完好");

        // 图元级往返（块引用字段）
        const QJsonObject ej = elementToJson(*m.element(rid));
        const Element back = elementFromJson(ej);
        check(back.type == ElementType::BlockRef && back.blockName == QStringLiteral("Ser"),
              "图元往返：BlockRef 类型与块名保持");
        check(near(back.scaleX, 1.5) && near(back.scaleY, 2.0) && near(back.rotation, 30.0),
              "图元往返：插入缩放/旋转保持");
        check(back.blockAttrs.size() == 1 && back.blockAttrs[0].value == QStringLiteral("hello"),
              "图元往返：属性值保持");
        check(back.dynValues.size() == 1 && back.dynValues[0].state == QStringLiteral("B"),
              "图元往返：动态参数（可见性状态）保持");

        // 旧存档兼容：无 blocks 字段 → 无块定义（不报错）
        SketchModel m3;
        m3.setBlocksFromJson(QJsonArray());
        check(m3.blockCount() == 0, "空块表读回：0 个块定义");
    }

    // ============================================================
    // 8. 撤销 / 重做
    // ============================================================
    printf("\n[8] 撤销与重做\n");
    {
        SketchModel m;
        const int l = m.addElement(mkLine(0, 0, 10, 0));
        m.setSelection({l});
        m.createBlockFromSelection(QStringLiteral("U"), Vec3(0, 0, 0), true);
        check(m.blockCount() == 1, "撤销前：块定义 1 个");
        m.undo();      // 撤销"创建块"（含删原图元 + 加引用）
        check(m.blockCount() == 0, "撤销创建块：块定义回退为 0");
        check(m.elementCount() == 1 && m.element(1) && m.element(1)->type == ElementType::Line,
              "撤销创建块：原直线恢复");
        m.redo();
        check(m.blockCount() == 1, "重做：块定义恢复");

        // 撤销插入
        const int before = m.elementCount();
        const int rid = m.insertBlock(QStringLiteral("U"), Vec3(50, 0, 0));
        check(rid >= 0 && m.elementCount() == before + 1, "插入块引用");
        m.undo();
        check(m.elementCount() == before, "撤销插入：引用被移除");

        // 撤销分解
        const int r2 = m.insertBlock(QStringLiteral("U"), Vec3(50, 0, 0));
        m.explodeBlockRef(r2);
        check(m.element(r2) == nullptr, "分解后原引用不存在");
        m.undo();
        check(m.element(r2) != nullptr && m.element(r2)->type == ElementType::BlockRef,
              "撤销分解：块引用恢复");
    }

    // ============================================================
    // 9. 边界情况
    // ============================================================
    printf("\n[9] 边界情况\n");
    {
        SketchModel m;
        // 插入不存在的块 → -1
        check(m.insertBlock(QStringLiteral("无"), Vec3(0, 0, 0)) < 0, "插入不存在的块返回 -1");
        // 未选择任何图元时建块 → 失败
        check(m.createBlockFromSelection(QStringLiteral("X"), Vec3(0, 0, 0), true) == -1,
              "无选择时创建块失败");
        // 空块定义（无图元）：可插入，展开为空
        BlockDef empty; empty.name = QStringLiteral("空块");
        m.addBlockDef(empty);
        const int re = m.insertBlock(QStringLiteral("空块"), Vec3(0, 0, 0));
        check(re >= 0 && m.expandBlockRef(*m.element(re)).empty(), "空块可插入，展开为空");
        // 缩放 0 被钳制（不把几何压成一点）
        BlockDef d; d.name = QStringLiteral("Z");
        d.entities = { mkLine(0, 0, 10, 0) };
        m.addBlockDef(d);
        const int rz = m.insertBlock(QStringLiteral("Z"), Vec3(0, 0, 0), 0.0, 0.0, 0.0);
        const auto exz = m.expandBlockRef(*m.element(rz));
        check(!exz.empty() && std::hypot(exz[0].pts[1].x - exz[0].pts[0].x,
                                        exz[0].pts[1].y - exz[0].pts[0].y) > 0.0,
              "缩放 0 被钳制为极小正值，几何不塌陷为一点");
        // 分解非块引用 → 失败
        const int ln = m.addElement(mkLine(0, 0, 1, 0));
        check(m.explodeBlockRef(ln).empty(), "分解非块引用失败");
        // undo 不越界（栈空安全）
        SketchModel me;
        me.undo();
        check(true, "空撤销栈 undo 不崩溃");
    }

    printf("\n块测试：%s（%d 项失败）\n", g_fail == 0 ? "全部通过" : "存在失败", g_fail);
    return g_fail == 0 ? 0 : 1;
}
