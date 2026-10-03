#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QHash>
#include <QList>
#include <QVector>
#include <QPair>
#include <QString>
#include <QColor>
#include "ui/LayerPanel.h"   // LayerPanel 定义（位于 namespace SketchCAD），供 layerPanel_ 成员使用
#include "ui/ConstraintBar.h" // ConstraintBar 定义（约束栏面板，需求③④）
#include "ui/DesignCenterPanel.h" // DesignCenterPanel 定义（设计中心侧栏，供 designPanel_ 成员使用）
#include "core/Vec.h"        // Vec3（onCanvasPointPicked 槽签名需要）
#include "core/Section.h"    // SectionStyle / SectionLoopSet（剖面填充点选模式的样式与缓存）
#include "ui/PrintEngine.h"  // PrintSettings（打印/出图设置，同时充当布局定义）
#include "ui/MTextDialog.h"       // MTEXT 多行文字对话框（双击编辑/菜单入口共用）
#include "ui/StyleManagerDialog.h" // STYLE 文字样式管理器对话框
#include "core/Clipboard.h"      // clipboard::Payload（剪贴板粘贴拾取状态成员需要）

class QDockWidget;
class QToolBar;
class QAction;
class QActionGroup;
class QMenu;
class QComboBox;
class QToolButton;
class QLabel;
class QDoubleSpinBox;
class QSpinBox;

namespace SketchCAD {
class SketchModel;
class CommandInterpreter;
class ScriptHost;
class PluginManager;
class GraphicsView;
class BomTree;
class ElementPanel;
class PropertyPanel;
class LogPanel;
class CommandLine;
class StatusBar;
class RulerWidget;
class Element;
class ArrayDialog;
class InsertBlockDialog;
class CreateBlockDialog;
class BlockManagerDialog;
class GroupManagerDialog;   // 编组管理器（GROUP）
class QSelectDialog;        // 快速选择（QSELECT）
class FilterDialog;         // 过滤器（FILTER）
struct SheetSpec;

// 主窗口：菜单栏、工具条、三栏 dock 布局、面板联动、文件 IO
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // "对齐方式"字符串 → modify::makeAlign 的 kind 整数：
    //   left / centerx / right / topy / centery / bottom → 0/1/2/3/4/5
    static int kindIndexFromName(const QString& k);

private:
    // 工具条分组：{分组标题, 该组的 QAction 列表}。
    // 工具条与「草图绘制」菜单复用同一批 QAction 指针，所以图标、快捷键、
    // 互斥勾选状态天然一致，改一处两处同步，不存在"菜单和工具条不同步"的问题。
    using ActionGroupList = QList<QPair<QString, QList<QAction*>>>;

    QDockWidget *bomDock_, *elemDock_, *propDock_, *logDock_, *cmdDock_, *layerDock_, *constraintDock_;
    QDockWidget* designDock_;                 // 设计中心（符号库浏览/插入）侧栏
    LayerPanel* layerPanel_;
    DesignCenterPanel* designPanel_;          // 设计中心面板（块表树 + 外部块库）
    ConstraintBar* constraintBar_;   // 约束栏面板（展示选中图元的约束与状态）
    // 工具条拆成两行，避免单行过长导致按钮被截断/挤压缩水/溢出窗口：
    //   toolbar_  = 第一行「草图工具」：选择 / 图元 / 曲线 / 标注 / 修改
    //   toolbar2_ = 第二行「样式与视图」：编辑 / 样式 / 视图 / 工作平面
    // 两者共用同一批 QAction，因此图标、提示、启用态、点击事件完全不受拆分影响。
    QToolBar* toolbar_  = nullptr;
    QToolBar* toolbar2_ = nullptr;
    QList<QToolBar*> customToolbars_;   // 由 CUIx 生成的自定义工具条（applyCui 时整体重建）
    QHash<QString, QAction*> toolActions_;   // 工具 id -> 工具条按钮（用于同步高亮）
    ActionGroupList drawGroups_;             // 绘图类工具（供菜单复用）
    ActionGroupList editGroups_;             // 编辑/视图类即时命令（供菜单复用）
    QAction*     actChain_   = nullptr;      // 连续直线模式开关
    QComboBox*   widthBox_   = nullptr;      // 线宽下拉
    QComboBox*   styleBox_   = nullptr;      // 线型下拉
    QComboBox*   stdBox_     = nullptr;      // 标注标准切换（GB/T 4458 ↔ ISO 129）
    QToolButton* colorBtn_   = nullptr;      // 颜色按钮
    QMenu*       widthMenu_  = nullptr;      // 菜单里的线宽子菜单
    QMenu*       colorMenu_  = nullptr;      // 菜单里的颜色子菜单
    QMenu*       dimMenu_    = nullptr;      // 菜单里的尺寸标注子菜单
    QList<QAction*> widthActs_, colorActs_;  // 子菜单项（用于勾选同步）
    QList<QAction*> dimActs_;                // 各类尺寸标注工具（不占工具条空间）

    SketchModel* model_;
    CommandInterpreter* interp_;
    ScriptHost* scriptHost_ = nullptr;   // 脚本宿主（二次开发：JS/Python/VBA 扩展）
    PluginManager* pluginManager_ = nullptr;   // 插件管理器（原生 SDK 二次开发）
    QMenu*       pluginLoadedSub_ = nullptr;   // 「已加载插件」动态子菜单
    GraphicsView* view_;

    PrintSettings currentPrintSettings_;   // 当前打印/出图设置（可由「打印设置」对话框修改并持久化）

    // 参数输入条控件
    QWidget*      paramBar_   = nullptr;
    QDoubleSpinBox* sbAngle_  = nullptr;
    QDoubleSpinBox* sbLength_ = nullptr;
    QDoubleSpinBox* sbSpacing_= nullptr;
    QSpinBox*     sbCount_    = nullptr;
    QComboBox*   paramStyleBox_ = nullptr;  // 参数输入条上的线型下拉
    QLabel*       paramHint_  = nullptr;
    QLabel*       lblAngle_   = nullptr;  // "角度" / 圆模式时隐藏
    QLabel*       lblLength_  = nullptr;  // "长度" / 圆模式时改为 "半径"
    QLabel*       lblCount_   = nullptr;  // "条数" / 多边形模式时改为 "边数"
    QLabel*       lblSpacing_ = nullptr;  // "间隔" / 多边形模式时隐藏

    BomTree* bom_;
    ElementPanel* elemPanel_;
    PropertyPanel* propPanel_;
    LogPanel* logPanel_;
    CommandLine* cmdLine_;
    StatusBar* statusBar_;
    RulerWidget* hRuler_ = nullptr;   // 顶部水平标尺
    RulerWidget* vRuler_ = nullptr;   // 左侧垂直标尺
    ArrayDialog* activeArrayDialog_ = nullptr;   // 当前打开的阵列对话框（用于视图拾取回填）

    // 镜像（点 / 线）交互状态机：选中图元 → 点「镜像」→ 选基准 → 自动生成镜像副本
    bool mirrorPending_ = false;   // 处于镜像基准拾取阶段
    bool mirrorIsPoint_  = false;  // true=点镜像；false=线镜像
    bool mirrorP1Valid_  = false;  // 线镜像：第一点已拾取
    Vec3 mirrorP1_;                // 线镜像：已拾取的第一点

    // 剪贴板（copybase / pasteclip / pasteblock）交互状态：
    //   copybase —— 拾取一个基点后按基点复制到剪贴板；
    //   粘贴     —— 拾取一个落点，把剪贴板基点对齐到该点后落盘。
    bool clipBasePending_ = false;          // 正在拾取 copybase 基点
    bool clipPastePending_ = false;         // 正在拾取粘贴落点
    bool clipPasteAsBlock_ = false;         // 粘贴落点拾取完成后是否转为块引用（pasteblock）
    clipboard::Payload clipPastePayload_;   // 粘贴阶段暂存的剪贴板载荷

    // ---- 剖面填充（区域拾取模式）状态 ----
    SectionStyle   sectionStyle_;            // 最近一次使用的剖面样式（点选填充复用）
    SectionLoopSet pickLoops_;               // 全图闭环集合缓存（惰性重建，避免逐帧解析）
    bool           pickLoopsValid_ = false;  // 缓存是否有效（模型变化 / 重新进入模式后失效）
    int            pickHlLoop_ = -2;         // 当前高亮的闭环下标（-2 = 尚未计算）

    void buildMenu();
    void buildSketchMenu(QMenu* m);           // 【草图绘制】菜单：镜像工具条全部绘图工具
    void buildLineMenu(QMenu* m);             // 【直线】菜单：直线绘制方式的中文二级子菜单
    void buildXlineMenu(QMenu* m);            // 【构造线】菜单：XLINE 六种绘制方式
    void buildCircleMenu(QMenu* m);           // 【圆】菜单：圆形绘制方式的中文二级子菜单
    void buildArcMenu(QMenu* m);              // 【圆弧】菜单：圆弧绘制方式的中文二级子菜单
    void buildPolyMenu(QMenu* m);             // 【多边形】菜单：多边形绘制方式的中文二级子菜单
    void buildSplineMenu(QMenu* m);           // 【样条】菜单：样条曲线绘制方式的中文二级子菜单
    void buildToolbar();
    // 统一单行工具条的外观（图标尺寸/按钮风格/内边距/间距/停靠约束）。
    // 两行都走这个函数，保证拆分后"两行看起来是同一条工具条"。
    static void applyToolBarLook(QToolBar* tb);
    // 统一按钮最小宽度：1~2 个汉字的按钮（圆/删除/XY…）宽度一致，排列整齐；
    // 更长的按钮（样条编辑）自然变宽，不会被截断。
    static void uniformButtonWidth(QToolBar* tb);
    void buildParamBar();                     // 工具条下方的参数输入条（角度/长度、平行间隔/条数）
    void updateParamBarForTool(const QString& t);  // 按当前命令启停参数输入条字段并给提示
    void applyUnitsToParamBar();              // 单位口径 → 参数条后缀与显示值（unitsChanged 时刷新）

    // ---- UI 工厂方法（Factory Pattern）----
    // 子模式定义：中文名、工具 ID、提示文本。直线/圆形子模式的唯一数据源。
    // instant=true 表示该项不是「可停留的工具模式」，而是点击即执行的一次性命令
    // （如 全选/全部不选/反选）：不进入互斥组、不高亮、不改变当前活动工具。
    struct ToolSubMode { QString zh; QString id; QString tip; bool instant = false; QString icon = QStringLiteral(""); };
    // icon：可选的菜单/子项图标 id（见 icons::make）。留空则不设置图标，
    // 保持既有下拉菜单（直线/圆等）的纯文字外观；选择类子模式逐项配图标，
    // 与参考图 LibreCAD Select 菜单"每项一带图标"的外观一致。
    // 创建带下拉菜单的工具按钮（直线/圆形共用同一套创建逻辑，消除重复代码）。
    // 主按钮触发 mainToolId，下拉菜单包含 subModes 中的每一项。
    void createDropdownTool(QToolBar* tb, QActionGroup* group, QList<QAction*>& bucket,
                            const QString& icon, const QString& btnText,
                            const QString& mainToolId, const QString& mainTip,
                            const QString& shortcut, const QString& menuTitle,
                            const QVector<ToolSubMode>& subModes);
    // 直线 / 圆形子模式定义的唯一数据源（工具条与菜单共用）
    static const QVector<ToolSubMode>& lineSubModes();
    static const QVector<ToolSubMode>& xlineSubModes();   // 构造线（Xline）子模式唯一数据源
    static const QVector<ToolSubMode>& circleSubModes();
    static const QVector<ToolSubMode>& arcSubModes();    // 圆弧子模式唯一数据源
    static const QVector<ToolSubMode>& polySubModes();   // 多边形子模式唯一数据源
    static const QVector<ToolSubMode>& splineSubModes();  // 样条绘制子模式（含自由手绘）唯一数据源
    static const QVector<ToolSubMode>& splineEditSubModes(); // 样条编辑命令唯一数据源
    static const QVector<ToolSubMode>& ellipseSubModes();   // 椭圆绘制子模式唯一数据源
    static const QVector<ToolSubMode>& polylineSubModes();  // 折线绘制+9 个编辑唯一数据源
    static const QVector<ToolSubMode>& selectSubModes();    // 选择菜单 8 项唯一数据源
    void buildEllipseMenu(QMenu* m);         // 【椭圆】菜单：与附图 FreeCAD Ellipse 菜单逐项一致
    void buildPolylineMenu(QMenu* m);        // 【折线】菜单：与附图 FreeCAD Polyline 菜单 10 项逐项一致
    void buildDocks();
    void connectSignals();

    // ---- 界面定制（CUIx + 命令别名，二次开发生态）----
    void initCui();                          // 注册内置 UI、载入默认别名/CUIx、首次应用
    void runToken(const QString& cmd);       // 自定义工具条按钮派发：工具→setActiveTool，否则 execute
    void applyCui();                         // 重建自定义工具条 + 应用当前工作空间可见性
    void rebuildCustomToolbars();            // 依据 CuiManager 重建自定义工具条（整体替换）
    void applyWorkspaceVisibility();         // 依据当前工作空间 show/hide 工具条与菜单
    void openAliasFile();                    // 打开/创建默认 .pgp（系统编辑器）
    void newToolbarDialog();                 // 新建自定义工具条对话框
    void saveCurrentAsWorkspaceDialog();     // 以当前可见 UI 保存为工作空间

    // 窗口显示时按其所在屏幕的可用区域（排除任务栏/系统栏）校正几何：
    // 宽/高不超过可用区域、整体居中于可用区域，确保任意常见分辨率下完整可见、
    // 左右边缘均不超出屏幕。构造函数里先给一个合理的默认几何，showEvent 中
    // 在真实屏幕上下文中再校正一次（避免窗口管理器首次 show 覆盖程序设定的位置）。
    void ensureVisibleOnScreen();
    void showEvent(QShowEvent* event) override;
    bool firstShow_ = true;     // 仅在首次 show 时校正几何，避免干扰用户后续的手动缩放/移动

    void saveFile();
    void openFile();
    void exportDxf();
    void changeDrawingScale();      // 修改绘图比例（图形:实际），并按比例缩放已有图元
    void syncTitleBlockScale();     // 把当前比例写进标题栏「比例」栏（tb:field:scale）
    void insertStandardTitleBlock(); // 插入标准标题栏：选文件→取元数据→在图框外追加标题栏（不改动已有图元）
    void generateFramedTitleBlock(); // 按幅面自动套框：选幅面→算内容包围盒→buildAutoFrameTitleBlock→只追加（不改动已有图元，标题栏自动避让）

    // 阵列复制：基于当前选择集生成矩形 / 环形阵列副本（原图元保留，副本继承全部属性）
    void arrayRectangular();   // 矩形阵列...：打开 ArrayDialog(矩形页) → makeRectangularArray
    void arrayPolar();         // 环形阵列...：打开 ArrayDialog(环形页) → makePolarArray（非模态，支持视图拾取中心）
    void arrayPath();          // 路径阵列...：沿选区中第一条 Polyline/Spline 等距布点 → makePathArray
    void onCanvasPointPicked(const Vec3& p);   // 视图拾取中心点完成后回填对话框并复位拾取态

    // 连合（JOIN）：把选中的同层 Line/Arc 按端点容差连合为 Polyline（GUI 传真实 snap 容差）
    void joinSelected();

    // ---- 剪贴板族（copyclip / copybase / pasteclip / pasteblock GUI 入口）----
    void copyClipToClipboard(bool withBase);   // withBase=false: 包围盒左下角为基点；true: 进入基点拾取
    void pasteFromClipboard(bool asBlock);     // asBlock=false: 平移粘贴；true: 转为块引用插入
    void handleClipPick(const Vec3& p);        // onCanvasPointPicked 的剪贴板拦截分支
    void finishClipPick();                     // 复位剪贴板拾取态与光标

    // 镜像（点 / 线）：选中图元 → 点「镜像」→ 选基准（点 / 直线）→ 自动生成镜像副本
    void mirror();                          // 入口：弹出"点镜像 / 线镜像"选择
    void handleMirrorPick(const Vec3& p);   // 拾取回调：收集基准（点 / 线的两点）并执行
    void applyMirror(bool isPoint, const Vec3& b1, const Vec3& b2);  // 生成镜像副本并写回模型
    void finishMirror();                    // 复位镜像交互状态与光标
    void onPointPickCancelled();            // 拾取阶段 Esc/右键取消

    // 剖面填充（GB/T 4457.5-2013）：进入「区域选择模式」——按鼠标点击位置自动识别
    // 该点所在的封闭区域并只填充它（相邻区域不受影响，嵌套闭环自动作内孔掏空）。
    void sectionFill();
    // 剖面填充参数：对**已填充的剖面区域（选中的 Hatch 图元）**就地重设材料类别/角度/间距，
    // 随时把「通用金属」换成「砖墙/石材/木材」等工程图案；无选中时仅更新默认样式。
    void sectionFillParams();
    void sectionFillSelected();                            // 用选中的边界/闭合图元填充（跳过自动追踪）
    void openRegionManager();                             // 边界区域(REGION)管理对话框
    void openCleanup();                                   // 图形清理（PURGE+OVERKILL）对话框
    void openInquiry();                                 // 测量查询面板入口（菜单 + 工具条共用）
    void openMTextDialog();                             // MTEXT 多行文字：新建/编辑入口
    void openStyleManager();                            // STYLE 文字样式管理器：创建/修改/删除/批量应用
    void editElementText(int id);                       // 双击 Text/MText/Table 进入编辑
    void openGroupManager();                            // GROUP 编组管理器（创建/加入/移除/整体选中）
    void openQSelect();                                 // QSELECT 快速选择（按类型/属性筛选落地选择集）
    void openFilterDialog();                            // FILTER 过滤器（多条件组合 + 保存复用）
    void openUnitsLimits();                             // UNITS/LIMITS 单位与图纸界限设置面板
    void openSnapSettings();                            // OSNAP 捕捉设置面板（全局持久配置）

    // ---- 脚本宿主（二次开发）----
    void onRunJavaScript();                             // 弹窗输入 JS 源码并执行
    void onRunScriptFile();                              // 选择脚本文件按扩展名执行

    // ---- 插件框架（原生 SDK 二次开发）----
    void onLoadPlugin();                                // 选择插件文件并加载
    void onReloadPlugins();                             // 重新加载插件目录
    void refreshPluginMenu();                           // 刷新「已加载插件」子菜单

    // ---- 打印 / 出图模块 ----
    void openPrintSettings();                            // 打印设置（页面 + CTB 样式 + 视口）
    void openPrintPreview();                             // 打印预览（QPrintPreviewWidget）
    void printDirect();                                 // 直接打印到打印机（模型空间）
    void exportPdfDirect();                             // 直接导出 PDF（QPdfWriter）
    void exportPdfLayoutsDirect();                      // 按布局表导出 PDF（每布局一页；空表回落模型空间单页）
    // ---- 区域拾取模式的响应（由 GraphicsView 的信号驱动）----
    void onRegionPickRequested(double x, double y);   // 左键点选：识别→填充该点所在封闭区域
    void onRegionHover(double x, double y);           // 光标移动：实时高亮"将要填充的区域"
    void onRegionPickCancelled();                     // Esc / 右键 / 切换工具 → 退出拾取模式
    void onRegionPickParams();                        // Enter → 打开剖面参数对话框
    void exitRegionPick(const QString& logMsg);       // 统一退出拾取模式（复位光标与高亮）
    void refreshPickLoopSet();                        // 惰性重建全图闭环集合缓存

    // ---- 块（Block）GUI 入口 ----
    // 把命令行 block create/insert/redefine/explode/delete/rename 封装为可视化操作。
    // 两个菜单（【编辑>修改>块】与【绘图>块】）共用同一个 buildBlockMenu 填充，
    // 保证两处入口完全一致。
    void buildBlockMenu(QMenu* m);                    // 填充一个「块(&B)」子菜单
    void openInsertBlockDialog();                     // 插入块...（带预览）
    void openCreateBlockDialog();                     // 创建块...
    void openBlockManagerDialog();                    // 块管理器...（重命名/删除/重定义）
    void openBlockEditorDialog();                     // BEDIT 在位编辑块（改一份定义 → 全部引用同步）
    void openDesignCenter();                          // 设计中心（符号库浏览/插入）侧栏
    void explodeBlockFromMenu();                      // 分解块...（当前选中的块引用）
    void openAttachXrefDialog();                      // 附加外部参照...（选择文件 + 路径类型）
    void detachXrefFromMenu();                        // 分离外部参照...（从已附加列表中选择）
    void insertBlockAtViewCenter(const QString& name); // 设计中心双击块 → 视图中心插入引用

    // ---- 样式（线宽 / 颜色）----
    void applyWidth(double px);               // 设默认线宽；若有选中则同时改选中图元
    void applyLineStyle(int idx);             // 设默认线型；若有选中则同时改选中图元
    void applyColor(const QColor& c);         // 设默认颜色；若有选中则同时改选中图元
    void pickCustomColor();
    void syncStyleWidgets(const QColor& c, double px);   // 模型 -> 各控件回显
    void syncLineStyleWidgets(int styleIdx);              // 线型 -> 各下拉框回显

    static const QVector<QPair<QString, QColor>>& presetColors();

    static QJsonObject elementToJson(const Element& e);
    static Element elementFromJson(const QJsonObject& o);

// ---- 菜单构建辅助（DRY：统一 build*Menu 中的重复子菜单 / 分组布局逻辑）----
    // 生成一个"带图标的二级子菜单"，并把 ids 中已注册的动作逐个加入。
    void addSubMenu(QMenu* parent, const QString& iconId,
                    const QString& title, const QStringList& ids);
    // 把若干组动作以"分隔线分组"扁平排入菜单（用于样条/椭圆/折线等菜单布局）。
    void addFlatGroups(QMenu* parent,
                       const std::initializer_list<QStringList>& groups);

public:
    // ---- 无对话框的可测接缝 ----
    // 上面的新建/打开/保存都靠模态对话框取参数，离屏测试无法驱动，导致这层胶水
    // 长期零覆盖（曾在一次重构中把 saveFile 的写盘段落误删，没有任何测试变红）。
    // 这里把「参数已经确定之后」那一段抽出来，测试即可跑完整往返：
    //   新建 → 改比例 → 存成 JSON/DXF/STEP/DWG → 再读回来 → 比对。
    SketchModel* model() const { return model_; }
    GraphicsView* view() const { return view_; }   // 供测试断言"打开后是否真的看得见图"
    void applyNewDrawing(const SheetSpec& spec, double scaleRatio);
    bool writeDrawing(const QString& path, QString* err = nullptr);   // 按扩展名选格式
    bool readDrawing(const QString& path, QString* err = nullptr);
    // 所有「非即时工具」子模式的工具 ID 汇总（去重）——UI 一致性测试的接缝：
    // 每个 UI 入口（下拉/菜单）的 id 都必须在 ToolRegistry 里有对应工具实现，
    // 否则 setActiveTool(id) 会静默失效（命令点了没反应）。
    // instant 项（全选/反选等）直接调用 model_，不依赖注册表，故不纳入。
    static QStringList allSubModeToolIds();
};
} // namespace SketchCAD
