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
#include "core/Vec.h"        // Vec3（onCanvasPointPicked 槽签名需要）
#include "core/Section.h"    // SectionStyle / SectionLoopSet（剖面填充点选模式的样式与缓存）

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

    QDockWidget *bomDock_, *elemDock_, *propDock_, *logDock_, *cmdDock_, *layerDock_;
    LayerPanel* layerPanel_;
    // 工具条拆成两行，避免单行过长导致按钮被截断/挤压缩水/溢出窗口：
    //   toolbar_  = 第一行「草图工具」：选择 / 图元 / 曲线 / 标注 / 修改
    //   toolbar2_ = 第二行「样式与视图」：编辑 / 样式 / 视图 / 工作平面
    // 两者共用同一批 QAction，因此图标、提示、启用态、点击事件完全不受拆分影响。
    QToolBar* toolbar_  = nullptr;
    QToolBar* toolbar2_ = nullptr;
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
    GraphicsView* view_;

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

    // ---- 剖面填充（区域拾取模式）状态 ----
    SectionStyle   sectionStyle_;            // 最近一次使用的剖面样式（点选填充复用）
    SectionLoopSet pickLoops_;               // 全图闭环集合缓存（惰性重建，避免逐帧解析）
    bool           pickLoopsValid_ = false;  // 缓存是否有效（模型变化 / 重新进入模式后失效）
    int            pickHlLoop_ = -2;         // 当前高亮的闭环下标（-2 = 尚未计算）

    void buildMenu();
    void buildSketchMenu(QMenu* m);           // 【草图绘制】菜单：镜像工具条全部绘图工具
    void buildLineMenu(QMenu* m);             // 【直线】菜单：直线绘制方式的中文二级子菜单
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
    void onCanvasPointPicked(const Vec3& p);   // 视图拾取中心点完成后回填对话框并复位拾取态

    // 剖面填充（GB/T 4457.5-2013）：进入「区域选择模式」——按鼠标点击位置自动识别
    // 该点所在的封闭区域并只填充它（相邻区域不受影响，嵌套闭环自动作内孔掏空）。
    void sectionFill();
    // ---- 区域拾取模式的响应（由 GraphicsView 的信号驱动）----
    void onRegionPickRequested(double x, double y);   // 左键点选：识别→填充该点所在封闭区域
    void onRegionHover(double x, double y);           // 光标移动：实时高亮"将要填充的区域"
    void onRegionPickCancelled();                     // Esc / 右键 / 切换工具 → 退出拾取模式
    void onRegionPickParams();                        // Enter → 打开剖面参数对话框
    void exitRegionPick(const QString& logMsg);       // 统一退出拾取模式（复位光标与高亮）
    void refreshPickLoopSet();                        // 惰性重建全图闭环集合缓存

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
