#include "CommandCatalog.h"
#include <QHash>
#include <algorithm>

namespace SketchCAD {

// ============================================================
// 静态元数据表
// ============================================================
// usage 字段来源（保证与真实解析行为同源）：
//   - 各命令实现的「用法: / 用法：」错误提示串；
//   - Commands.h 家族注释（xline/ucs/layer/block/xref/clip 等分派命令）。
// 未列出的命令不编造用法，help <cmd> 会走回落分支。
// ============================================================

const QVector<CommandInfo>& CommandCatalog::entries() {
    // ⚠️ 用 add() 逐条构建，而不是一条巨型聚合初始化：
    //    MinGW GCC 13.1 (cc1plus) 处理约 47+ 条非平凡类型嵌套花括号初始化
    //    会静默崩溃（EXIT=1 零诊断）；逐条 add 是独立表达式，彻底绕开。
    static const QVector<CommandInfo> table = [] {
        QVector<CommandInfo> t;
        auto add = [&t](const char* name, const char* zh, const char* cat,
                        const char* sum, const char* usage, const char* params,
                        const char* ex, std::initializer_list<const char*> rel,
                        const char* sc) {
            CommandInfo ci;
            ci.name = QString::fromUtf8(name);
            ci.zhName = QString::fromUtf8(zh);
            ci.category = QString::fromUtf8(cat);
            ci.summary = QString::fromUtf8(sum);
            ci.usage = QString::fromUtf8(usage);
            ci.params = QString::fromUtf8(params);
            ci.example = QString::fromUtf8(ex);
            for (const char* r : rel) ci.related << QString::fromUtf8(r);
            ci.shortcut = QString::fromUtf8(sc);
            t.append(ci);
        };
    add("line", "直线", "绘图", "两点绘制直线", "line x1,y1 x2,y2", "x1,y1 起点；x2,y2 终点（点格式 x,y 或 x y 或 x,y,z）", "line 0,0 100,50", { "line_h", "line_v", "line_angle" }, "");
    add("circle", "圆", "绘图", "圆心 + 半径绘制圆", "circle cx,cy r", "cx,cy 圆心；r 半径（>0）", "circle 50,50 25", { "circle3p", "arc" }, "");
    add("circle3p", "三点圆", "绘图", "过三点绘制圆", "circle3p x1,y1 x2,y2 x3,y3", "", "circle3p 0,0 40,0 20,30", { "circle" }, "");
    add("arc", "圆弧", "绘图", "圆心 + 半径 + 起止角绘制圆弧", "arc cx,cy r a0 [a1]", "a0 起始角；a1 终止角（度，省略 = a0+360 即整圆弧）", "arc 0,0 30 0 90", { "circle", "circle3p" }, "");
    add("rect", "矩形", "绘图", "两角点绘制矩形", "rect x0,y0 x1,y1", "", "rect 0,0 80,50", { "rect_chamfer", "rect_fillet", "poly" }, "");
    add("poly", "多边形", "绘图", "依次输入顶点绘制多边形（首末自动闭合）", "poly x1,y1 x2,y2 ...", "", "poly 0,0 40,0 20,35", { "poly_regular", "poly_edge" }, "");
    add("spline", "样条曲线", "绘图", "依次输入顶点绘制样条曲线", "spline p1 p2 ...", "", "spline 0,0 30,20 60,5", { "spline_parabola_4p", "spline_through_points" }, "");
    add("point", "点", "绘图", "绘制单个或多个点", "point <x,y> [x,y ...]", "", "point 10,10 30,40", {}, "");
    add("xh", "序号球标", "绘图", "为选中对象生成序号球标并写入明细表（BOM）", "xh [id ...]", "先选中零部件几何或给定图元 id；自动按质心放置圆+编号球标并追加明细行", "xh 12 13", { "bom", "balloon" }, "");
    add("xline", "构造线", "绘图", "无限长构造直线（过点+角度 / 两点 / 平行 / 平分）", "xline x,y <角度> | xline x1,y1 x2,y2", "子模式：xline_h / xline_v / xline_angle / xline_bisector / xline_parallel", "xline 0,0 45", { "line" }, "");
    add("hatch", "图案填充", "绘图", "对选中闭合边界生成关联填充，或调整填充参数", "hatch [scale <倍数> | angle <度> | pattern <名|none>]", "先选中闭合边界图元（圆/矩形/闭合折线等）；无参数 = 默认填充", "hatch scale 1.5", { "bhatch", "hatchedit" }, "");
    add("bhatch", "边界填充", "绘图", "hatch 的别名入口（自动选边界）", "bhatch", "", "", { "hatch" }, "");
    add("aircraft_views", "飞机三面图", "绘图", "按当前参数一键生成航空三面图模板", "aircraft_views", "", "", {}, "");
    add("line_h", "水平线", "绘图", "过点绘制水平直线", "line_h x,y <长度>", "", "line_h 10,10 50", { "line_v" }, "");
    add("line_v", "竖直线", "绘图", "过点绘制竖直直线", "line_v x,y <长度>", "", "line_v 10,10 40", { "line_h" }, "");
    add("line_angle", "定角线", "绘图", "过点按角度与长度绘制直线", "line_angle x,y <角度> <长度>", "", "line_angle 0,0 30 60", { "line_rel_angle" }, "");
    add("spline_parabola_4p", "四点抛物线", "绘图", "过四点构造抛物线样条", "spline_parabola_4p x1,y1 x2,y2 x3,y3 x4,y4", "", "", { "spline" }, "");
    add("spline_through_points", "过点样条", "绘图", "依序通过给定点列构造样条", "spline_through_points x1,y1 x2,y2 ...", "", "", { "spline" }, "");
    add("spline_from_polyline", "折线转样条", "绘图", "把折线图元转换为样条", "spline_from_polyline <id>", "", "", { "spline_explode" }, "");
    add("spline_explode", "样条打散", "修改", "把样条打散为折线/线段", "spline_explode <id>", "", "", { "spline_from_polyline", "explode" }, "");
    add("move", "移动", "修改", "平移选中图元", "move <dx> <dy> [copy]", "", "move 10 0", { "move_copy", "rotate", "scale" }, "");
    add("move_copy", "复制移动", "修改", "移动的复制版：原对象保留", "move_copy <dx> <dy>", "", "", { "move" }, "");
    add("rotate", "旋转", "修改", "绕基点旋转选中图元", "rotate <基点x,y> <角度> [copy]", "角度为度，逆时针为正", "rotate 0,0 90", { "scale", "mirror" }, "");
    add("scale", "缩放", "修改", "绕基点缩放选中图元", "scale <基点x,y> <比例> [copy]", "", "scale 0,0 2", { "rotate" }, "");
    add("mirror", "镜像", "修改", "沿镜像轴复制/翻转选中图元", "mirror x1,y1 x2,y2 [delete源]", "", "mirror 0,0 100,0", { "rotate" }, "");
    add("offset", "偏移", "修改", "按距离偏移直线/圆/弧/折线", "offset <id> <距离> [方向点x,y]", "", "offset 3 8", { "polyline_equidistant" }, "");
    add("trim", "修剪", "修改", "以边界修剪图元（无边界 = 全体为界）", "trim <id> <x,y> [边界id...]", "x,y 为拾取点（点选被剪一侧）", "trim 5 40,10", { "break_at", "lengthen" }, "");
    add("break_at", "打断", "修改", "在指定点把图元一分为二", "break_at <id> <x,y>", "", "", { "trim", "divide" }, "");
    add("divide", "定数等分", "修改", "把图元等分为 n 段", "divide <id> <n>", "", "", { "measure" }, "");
    add("measure", "定距等分", "修改", "按间距在图元上放置等分点", "measure <id> <间距>", "", "measure 3 10", { "divide" }, "");
    add("lengthen", "拉长", "修改", "按增量/百分比拉长或缩短图元", "lengthen <id> <参数>", "", "", { "trim" }, "");
    add("fillet", "圆角", "修改", "对两图元倒圆角", "fillet <id1> <id2> <半径>", "", "fillet 1 2 5", { "chamfer" }, "");
    add("chamfer", "倒角", "修改", "对两图元倒角（距离-距离 / 距离-角度）", "chamfer <id1> <id2> <距离1> <距离2|角度>", "", "", { "fillet" }, "");
    add("explode", "分解", "修改", "把块引用/复合图元分解为独立图元", "explode <id>", "", "", { "block", "spline_explode" }, "");
    add("align", "对齐", "修改", "按源点/目标点对对齐并可选缩放", "align <id> <源1> <目标1> [<源2> <目标2>]", "", "", { "move", "rotate" }, "");
    add("stretch", "拉伸", "修改", "拉伸选中图元的指定部分", "stretch <id> <参数>", "", "", { "move" }, "");
    add("join", "连合", "修改", "把同层首尾相接的 Line/Arc 连合为 Polyline", "join [容差mm]", "作用于当前选择集；容差缺省 0.5mm", "join 0.5", { "pedit" }, "");
    add("arrayrect", "矩形阵列", "修改", "选中图元按行列矩形阵列", "arrayrect <行数> <列数> <行距> <列距>", "作用于当前选择集", "arrayrect 2 3 10 20", { "arraypolar", "arraypath" }, "");
    add("arraypolar", "环形阵列", "修改", "选中图元绕中心环形阵列（基点默认捕捉圆心）", "arraypolar <中心x,y> <个数> [填充角]", "作用于当前选择集；选中圆/弧时可省略中心=圆心", "arraypolar 0,0 4 180", { "arrayrect", "arraypath" }, "");
    add("arraypath", "路径阵列", "修改", "沿 Polyline/Spline 路径阵列选中图元", "arraypath <路径id> <间距> | arraypath divide <路径id> <数量>", "作用于当前选择集；spacing=0 时用 divide 等分全长", "arraypath 7 25", { "arrayrect", "arraypolar" }, "");
    add("matchprop", "特性匹配", "修改", "把源图元的图层/颜色/线宽/线型复制到目标", "matchprop <源id> <目标id...>", "", "matchprop 1 2 3", { "properties" }, "");
    add("delete", "删除", "修改", "删除指定图元（或当前选择集）", "delete [id...]", "", "delete 7", { "undo" }, "");
    add("undo", "撤销", "修改", "撤销上一步编辑", "undo", "", "", { "redo" }, "Ctrl+Z");
    add("redo", "重做", "修改", "恢复被撤销的编辑", "redo", "", "", { "undo" }, "Ctrl+Y");
    add("clear", "清空", "修改", "清空图中全部图元", "clear", "", "", {}, "");
    add("list", "列表", "查询", "列出图元的类型/图层/几何信息", "list [id...]", "无 id = 全部图元", "list 3", { "dist", "area" }, "");
    add("dist", "距离", "查询", "测量两点距离与角度", "dist x1,y1 x2,y2", "", "dist 0,0 30,40", { "area", "id" }, "");
    add("area", "面积", "查询", "测量封闭图元或多点围成区域面积", "area <封闭图元id> | x1,y1 x2,y2 [x3,y3...]", "", "area 5", { "massprop", "dist" }, "");
    add("massprop", "面域特性", "查询", "封闭区域的面积/质心/惯性矩等", "massprop <封闭图元id> ...", "", "", { "area" }, "");
    add("id", "点坐标", "查询", "显示指定点坐标", "id x,y", "", "id 12.5,7", {}, "");
    add("fieldupdate", "域更新", "查询", "全模型重算 FIELD 域（MText/Table 预热求值，异常回落 ###）", "fieldupdate", "无参数；打开文档自动刷新见选项 fields/autoUpdateOnOpen", "fieldupdate", { "prop", "area" }, "");
    add("prop", "图纸特性", "查询", "自定义图纸特性的设置/查询/删除（PROP 域数据源）", "prop [<名称> [<值>] | -r <名称>]", "无参 = 列出全部；MText/Table 中以 %<PROP:名称>% 引用", "prop 项目名称 减速器箱体", { "fieldupdate", "mtext" }, "");
    add("qselect", "快速选择", "查询", "按类型/图层/颜色/线型四维过滤并选中图元", "qselect [type=类型] [layer=图层] [color=#rrggbb] [linestyle=线型]", "任一维省略 = 不过滤该维；全空 = 仅提示", "qselect type=直线 layer=0", { "filter" }, "");
    add("filter", "选择过滤器", "查询", "创建/应用命名选择过滤器", "filter [type=类型] [layer=图层] ... | none", "", "", { "qselect" }, "");
    add("dim", "尺寸标注", "标注", "线性/对齐/基线/连续/角度/半径/直径/引线/坐标标注（GB/T 4458.4-2003）", "dim linear|aligned|h|v x1,y1 x2,y2 [px,py]", "子型：baseline / continue / angle / radius / dia / arc / leader / ordinate", "dim aligned 0,0 100,0", { "dims", "dimstyle" }, "");
    add("dims", "智能标注", "标注", "依据当前选区自动选择标注类型（直线→对齐，圆→直径）", "dims [id...]", "", "", { "dim" }, "");
    add("dimstyle", "标注样式", "标注", "标注样式的创建/切换/应用/删除（一图多标准）", "dimstyle [list | new <名> [gbt|iso] | set <名> | rename <旧> <新> | apply <名> [id...] | delete <名>]", "", "dimstyle new GBT gbt", { "dim" }, "");
    add("qdim", "快速标注", "标注", "框选平行线/圆阵列，一次性生成基线或连续标注串（复用 dim 基线/连续引擎）", "qdim [baseline|continue]", "先框选一组平行线或圆阵列；baseline=共基点基线，continue=链式连续", "qdim continue", { "dim", "dimstyle" }, "");
    add("tol", "形位公差", "标注", "在指定位置放置形位公差框格", "tol <x,y> [公差文本]", "", "tol 50,30", { "dim" }, "");
    add("constraint", "草图约束", "约束", "几何约束的添加/求解/检查/诊断/清理", "constraint add <类型> <idA> <idB> [值|轴id] | solve | list | check | diagnose | clear", "类型：fixed/fixedpoint/horizontal/vertical/parallel/perpendicular/tangent/...", "constraint add horizontal 1", { "perp", "tangent2", "qconnect" }, "");
    add("perp", "垂线生成", "约束", "过指定点作直线垂直于指定直线", "perp <直线Id> <x,y>", "", "perp 1 30,20", { "constraint" }, "");
    add("tangent2", "公切线", "约束", "求两圆/弧的全部公切线（外+内，0..4 条）", "tangent2 <圆IdA> <圆IdB>", "", "tangent2 1 2", { "constraint" }, "");
    add("qconnect", "象限连接", "约束", "从一点连到目标圆/弧最近的象限点", "qconnect <目标圆Id> <x,y>", "", "", { "constraint" }, "");
    add("select_all", "全部选择", "选择", "选中图中全部图元", "select_all", "", "", { "select_none" }, "Ctrl+A");
    add("select_none", "放弃选择", "选择", "清空当前选择集", "select_none", "", "", { "select_all" }, "Esc");
    add("invert", "反选", "选择", "选择集取反", "invert", "", "", {}, "");
    add("select_entity", "选择图元", "选择", "把指定 id 加入选择集", "select_entity <id>", "", "", { "deselect_entity" }, "");
    add("deselect_entity", "取消选择图元", "选择", "把指定 id 移出选择集", "deselect_entity <id>", "", "", { "select_entity" }, "");
    add("select_layer", "按层选择", "选择", "选中指定图层上的全部图元", "select_layer <图层名>", "", "", { "deselect_layer", "layiso" }, "");
    add("deselect_layer", "取消按层选择", "选择", "把指定图层图元移出选择集", "deselect_layer <图层名>", "", "", { "select_layer" }, "");
    add("layer", "图层", "图层", "图层的增删改/显隐/锁定/赋层/套用/列表（单一分派命令）", "layer add <名称> [颜色] [线宽] [线型] | delete | rename | visible | lock | set | assign | apply | list", "", "layer add 中心线 red 0.25 CENTER", { "layiso", "layuniso", "laymch" }, "");
    add("layiso", "图层隔离", "图层", "只保留选中图元所在图层可见", "layiso", "", "", { "layuniso", "layer" }, "");
    add("layuniso", "取消隔离", "图层", "恢复被隔离图层的显示", "layuniso", "", "", { "layiso" }, "");
    add("laymch", "图层匹配", "图层", "把选中图元移到目标图层", "laymch <目标图层名>", "", "", { "layer" }, "");
    add("ltscale", "线型比例", "图层", "设置全局线型比例（单一 LTSCALE）", "ltscale <正数>", "", "ltscale 0.5", {}, "");
    add("block", "块", "块", "块的创建/插入/重定义/分解/删除/重命名/列表（单一分派命令）", "block create <块名> [基点x,y] [delete|keep] | insert <块名> <x,y> [旋转] [sx] [sy] | list | info <块名> | redefine | explode <引用id> | delete | rename", "", "block create 法兰 0,0", { "insert", "explode", "attdef" }, "");
    add("insert", "插入块", "块", "把块定义插入为块引用", "insert <块名> <x,y> [旋转度] [缩放X] [缩放Y]", "", "insert 法兰 50,50 0", { "block" }, "");
    add("attdef", "属性定义", "块", "为块定义追加属性定义（属性块）", "attdef <块名> <标签> [提示] [x,y] [字高]", "", "", { "attedit", "attlist" }, "");
    add("attedit", "编辑属性", "块", "编辑块引用的属性值", "attedit <引用id> <标签> <值>", "", "", { "attdef" }, "");
    add("attlist", "属性提取", "块", "列出块引用属性（无 id = 全部引用）", "attlist [引用id]", "", "", { "attedit" }, "");
    add("dynset", "动态块参数", "块", "设置块引用的动态参数（数值或可见性状态）", "dynset <引用id> <参数名> <值|状态名>", "", "", { "dynlist" }, "");
    add("dynlist", "动态参数列表", "块", "列出块定义的动态参数", "dynlist <块名>", "", "", { "dynset" }, "");
    add("wblock", "写块", "块", "把图元/选择集导出为独立 DXF 文件", "wblock <输出.dxf> [id...]", "", "wblock part.dxf", { "dxfout" }, "");
    add("xref", "外部参照", "块", "外部参照的附加/分离/列表/路径诊断", "xref attach <文件路径> [full|relative|none] [x,y] [旋转] [sx] [sy] | detach <块名> | list | path <块名>", "", "", { "block" }, "");
    add("copyclip", "复制", "修改", "把当前选择集写入内部剪贴板（基点 = 包围盒左下角）", "copyclip", "", "", { "pasteclip" }, "Ctrl+C");
    add("copybase", "带基点复制", "修改", "复制但基点由用户指定（AutoCAD COPYBASE 语义）", "copybase <x,y>", "", "", { "copyclip" }, "Ctrl+Shift+C");
    add("pasteclip", "粘贴", "修改", "把剪贴板内容粘贴为独立图元", "pasteclip [x,y]", "", "", { "copyclip" }, "Ctrl+V");
    add("pasteblock", "粘贴为块", "修改", "把剪贴板内容整包收为块定义并放置引用", "pasteblock <x,y>", "", "", { "pasteclip" }, "Ctrl+Shift+V");
    add("plane", "视图平面", "视图", "切换绘图平面", "plane xy|yz|xz", "", "plane xy", { "ucs" }, "");
    add("zoom", "缩放", "视图", "按倍率缩放视图", "zoom <倍率>", "", "zoom 2", {}, "");
    add("grid", "栅格", "视图", "栅格显示开关", "grid [on|off]", "", "", {}, "F9");
    add("regen", "重生成", "视图", "重建空间索引并重绘视图", "regen", "", "", {}, "F5");
    add("layout", "布局", "布局", "布局的增删改/切换/视口比例/冻结（单一分派命令）", "layout new/set/list/rename/delete/copy/vpscale/vpfreeze/vpthaw", "", "layout new A3", { "mview" }, "");
    add("mview", "建视口", "布局", "在当前布局图纸坐标建矩形视口", "mview <图纸X> <图纸Y> <宽> <高> [比例]", "", "mview 10 10 90 60 1:2", { "layout" }, "");
    add("ucs", "用户坐标系", "视图", "UCS 的定义/切换/命名管理（单一分派命令）", "ucs world|view|3p <p1> <p2> [p3]|object <id>|z <x,y> <角>|origin <x,y> [角]|save/set/restore/delete/list|coords wcs|ucs", "", "ucs 3p 0,0 100,0", { "plane" }, "");
    add("width", "线宽", "样式", "设置后续绘制线宽档位", "width 1|2|3|5", "", "", {}, "");
    add("color", "颜色", "样式", "设置后续绘制颜色", "color #RRGGBB|颜色名", "", "color red", {}, "");
    add("chain", "连续绘制", "样式", "连续绘制开关（上一图元终点为下一图元起点）", "chain [on|off]", "", "", {}, "K");
    add("ortho", "正交", "追踪", "正交模式开关", "ortho [on|off]", "", "", {}, "F8");
    add("snap", "捕捉", "追踪", "捕捉开关或设置步长", "snap [on|off|<步长mm>]", "", "", {}, "");
    add("polar", "极轴追踪", "追踪", "极轴追踪开关或设置增量角", "polar [on|off|<增量角>]", "", "", {}, "F10");
    add("otrack", "对象捕捉追踪", "追踪", "对象捕捉追踪开关", "otrack [on|off]", "", "", {}, "F11");
    add("dyn", "动态输入", "追踪", "动态输入开关", "dyn [on|off]", "", "", {}, "F12");
    add("tk", "临时追踪点", "追踪", "放置临时追踪点", "tk <x,y>", "", "", { "tkclear" }, "");
    add("tkclear", "清除追踪点", "追踪", "清除全部临时追踪点", "tkclear", "", "", { "tk" }, "");
    add("pedit", "多段线编辑", "工具", "折线的闭合/线宽/连合/拟合/样条化", "pedit <id> [close|open|width <线宽>|join <id...>|fit|spline]", "", "", { "join", "polyline" }, "");
    add("properties", "特性", "工具", "打开特性面板编辑选中图元", "properties（先选中）或 properties <id...>", "", "", { "matchprop" }, "Ctrl+1");
    add("purge", "清理", "工具", "清理未引用的图层/块/标注样式", "purge [all|layers|blocks|dimstyles]", "", "", {}, "");
    add("overkill", "删除重复对象", "工具", "合并/删除重复或重叠图元", "overkill", "", "", {}, "");
    add("text", "单行文字", "工具", "放置单行文字", "text <x,y> <字高> <内容...>", "", "text 0,0 3.5 说明", { "dtext", "mtext", "ddedit" }, "");
    add("dtext", "单行文字", "工具", "text 的别名入口", "dtext <x,y> <字高> <内容...>", "", "", { "text" }, "");
    add("mtext", "多行文字", "工具", "放置多行文字", "mtext <x,y> <字高> <内容...>", "", "", { "text" }, "");
    add("ddedit", "编辑文字", "工具", "修改既有文字内容", "ddedit <id> <新内容>", "", "", { "text", "mtext" }, "");
    add("dimedit", "尺寸驱动", "标注", "修改标注数值，几何按关联联动调整，标注/文字/域同步更新", "dimedit <标注id> <新值> 或选中标注后 dimedit <新值>", "支持：对齐/距离/水平/竖直/半径/直径/角度；容忍 R/⌀/° 前后缀", "dimedit 50", { "dim", "ddedit" }, "");
    add("linetype", "线型", "工具", "线型的加载/设置/列表", "linetype", "", "", {}, "");
    add("dxfout", "导出 DXF", "工具", "把当前图导出为 DXF 文件（可选版本）", "dxfout <路径.dxf> [-v R12|2000]", "", "dxfout part.dxf -v 2000", { "wblock" }, "");
    add("group", "编组", "工具", "编组的创建/加成员/移除/列表/解组", "group [create <名> | add <名> | remove | list | ungroup <名>]", "", "group create 螺栓组", { "block" }, "");
    add("macrorec", "录制宏", "脚本", "开始录制命令宏", "macrorec", "", "", { "macrostop" }, "");
    add("macrostop", "停止录制", "脚本", "结束宏录制", "macrostop", "", "", { "macrorec" }, "");
    add("macrosave", "保存宏", "脚本", "把录制的宏保存为文件", "macrosave \"<路径>\"", "", "", { "macroplay" }, "");
    add("macroplay", "回放宏", "脚本", "回放已保存的宏", "macroplay \"<路径>\"", "", "", { "macrosave", "scr" }, "");
    add("scr", "运行脚本", "脚本", "逐行执行脚本文件中的命令", "scr \"<路径>\"", "", "", { "macroplay" }, "");
    add("alias", "别名", "工具", "新增/覆盖命令别名（acad.pgp 兼容）", "alias <别名> <完整命令>", "", "alias cc circle", { "aliaslist", "aliasreload" }, "");
    add("aliasreload", "重载别名", "工具", "从 .pgp 文件重新加载别名表", "aliasreload", "", "", { "alias" }, "");
    add("aliasload", "加载别名", "工具", "从指定 .pgp 文件加载别名", "aliasload \"<路径>\"", "", "", { "aliasreload" }, "");
    add("aliassave", "保存别名", "工具", "把当前别名表写回 .pgp 文件", "aliassave [\"<路径>\"]", "", "", { "aliasload" }, "");
    add("aliaslist", "别名列表", "工具", "列出当前全部别名", "aliaslist", "", "", { "alias" }, "");
    add("cuiload", "加载 CUIx", "工具", "加载界面定制文件（CUIx）", "cuiload \"<路径>\"", "", "", { "cuisave" }, "");
    add("cuisave", "保存 CUIx", "工具", "保存界面定制文件", "cuisave \"<路径>\"", "", "", { "cuiload" }, "");
    add("wslist", "工作空间列表", "工具", "列出可用工作空间", "wslist", "", "", { "wsload" }, "");
    add("wsload", "加载工作空间", "工具", "切换到指定工作空间", "wsload <名>", "", "", { "wslist" }, "");
    add("help", "帮助", "帮助", "命令总表；help <命令> 查看用法/参数/样例", "help [<命令名>]", "", "help line", {}, "F1");
        return t;
    }();
    return table;
}

const CommandInfo* CommandCatalog::find(const QString& name) {
    const QString key = name.toLower();
    for (const CommandInfo& ci : entries())
        if (ci.name == key) return &ci;
    return nullptr;
}

QString CommandCatalog::categoryOf(const QString& name) {
    if (const CommandInfo* ci = find(name)) return ci->category;
    const QString n = name.toLower();
    if (n.startsWith(QLatin1String("line_")) || n.startsWith(QLatin1String("spline_")) ||
        n.startsWith(QLatin1String("ellipse_")) || n.startsWith(QLatin1String("polyline_")) ||
        n == QLatin1String("polyline") || n == QLatin1String("freehand_line"))
        return QStringLiteral("绘图");
    if (n.startsWith(QLatin1String("select_")) || n.startsWith(QLatin1String("deselect_")) ||
        n == QLatin1String("invert") || n == QLatin1String("toggle_contour") ||
        n == QLatin1String("toggle_layer"))
        return QStringLiteral("选择");
    if (n == QLatin1String("ellipse_1pt") || n.startsWith(QLatin1String("ellipse")))
        return QStringLiteral("绘图");
    return QStringLiteral("其他");
}

// ============================================================
// 命令面板纯逻辑层
// ============================================================

void CommandPaletteModel::pushRecent(const QString& name, int maxKeep) {
    const QString n = name.trimmed().toLower();
    if (n.isEmpty()) return;
    recent_.removeAll(n);
    recent_.prepend(n);
    while (recent_.size() > maxKeep) recent_.removeLast();
}

// 子序列匹配：query 的字符按序全部出现在 s 中（大小写不敏感）。
// 返回间隔代价（∑ 相邻命中字符间的跳过字符数），不匹配返回 -1。
static int subsequenceCost(const QString& s, const QString& q) {
    int cost = 0, idx = -1;
    for (const QChar qc : q) {
        const int next = s.indexOf(qc, idx + 1, Qt::CaseInsensitive);
        if (next < 0) return -1;
        cost += (idx >= 0) ? (next - idx - 1) : 0;
        idx = next;
    }
    return cost;
}

int CommandPaletteModel::fuzzyScore(const QString& query, const PaletteEntry& e) {
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) return 0;
    int best = -1;
    const QVector<QPair<QString, int>> channels = {
        { e.name.toLower(), 100 },
        { e.title.toLower(), 90 },
        { e.alias.toLower(), 80 },
    };
    for (const auto& ch : channels) {
        if (ch.first.isEmpty()) continue;
        int score = -1;
        if (ch.first == q) {
            score = ch.second * 2;                       // 完全相等最高
        } else if (ch.first.startsWith(q)) {
            score = qRound(ch.second * 1.2);             // 前缀匹配
        } else if (ch.first.contains(q, Qt::CaseInsensitive)) {
            score = ch.second;                           // 包含匹配
        } else {
            const int cost = subsequenceCost(ch.first, q);
            if (cost >= 0 && cost <= 8)                  // 子序列：间隔过大视为不匹配
                score = ch.second - cost * 5;
        }
        if (score > best) best = score;
    }
    return best;
}

QVector<PaletteEntry> CommandPaletteModel::filter(const QString& query) const {
    const QString q = query.trimmed();
    // 命中条目集合（去重：同一命令名的别名条目不重复占用展示位时，
    // 仍允许并存——别名入口与命令本体都是合法可执行路径）。
    QVector<PaletteEntry> hits;
    if (q.isEmpty()) {
        hits = entries_;
        // 最近使用置顶：按 recent_ 顺序提升；其余保持原序（名称/注册序）。
        int insertPos = 0;
        for (const QString& r : recent_) {
            for (int i = insertPos; i < hits.size(); ++i) {
                if (hits[i].name == r) {
                    const PaletteEntry e = hits.takeAt(i);
                    hits.insert(insertPos++, e);
                    break;
                }
            }
        }
        return hits;
    }
    struct Scored { int score; int order; PaletteEntry e; };
    QVector<Scored> scored;
    for (int i = 0; i < entries_.size(); ++i) {
        const int s = fuzzyScore(q, entries_[i]);
        if (s > 0) scored.append({ s, i, entries_[i] });
    }
    // 得分降序；同分保持注册/装配顺序（stable_sort 语义）。
    std::stable_sort(scored.begin(), scored.end(),
                     [](const Scored& a, const Scored& b) { return a.score > b.score; });
    for (const auto& sc : scored) hits.append(sc.e);
    return hits;
}

// ============================================================
// 快捷键冲突自检
// ============================================================

namespace ShortcutAudit {

QVector<QPair<QString, QString>> duplicates(const QVector<Binding>& bindings) {
    QHash<QString, QVector<int>> byKey;   // 规格化快捷键 → 参与冲突的 Binding 下标
    for (int i = 0; i < bindings.size(); ++i) {
        const QString key = bindings[i].shortcut.trimmed();
        if (key.isEmpty()) continue;
        byKey[key.toLower()] << i;
    }
    QVector<QPair<QString, QString>> out;
    // 输出按原始注册顺序（首个冲突成员的出现序）。
    QHash<QString, bool> reported;
    for (int i = 0; i < bindings.size(); ++i) {
        const QString key = bindings[i].shortcut.trimmed();
        if (key.isEmpty()) continue;
        const QString norm = key.toLower();
        const QVector<int>& members = byKey[norm];
        if (members.size() < 2 || reported.value(norm)) continue;
        reported[norm] = true;
        QStringList actions;
        for (int idx : members) actions << bindings[idx].action;
        out.append({ key, actions.join(QStringLiteral(" / ")) });
    }
    return out;
}

} // namespace ShortcutAudit

} // namespace SketchCAD
