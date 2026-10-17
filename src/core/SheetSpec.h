#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QRectF>
#include <optional>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

// 图纸幅面（GB/T 14689 / ISO 128 系列 A 系列）
enum class SheetFormat { A0, A1, A2, A3, A4, A5 };

// 投影法（GB/T 17451 / GB/T 14665-2012 第 7 章 图形符号 表5）
// 第一角画法：俯视图在主视图下方、左视图在主视图右方（国标默认）；
// 第三角画法：俯视图在主视图上方、左视图在主视图左方（与 ISO 一致）。
enum class ProjectionMethod { FirstAngle, ThirdAngle };

// 新建图纸的标题栏样式（只增不改：Standard=现状，None=仅图框不画标题栏）
enum class TitleBlockStyle { Standard, None };

// 图纸规格：含幅面、横竖方向与最终毫米尺寸
struct SheetSpec {
    SheetFormat format = SheetFormat::A3;
    bool landscape = true;       // true = 长边水平（工程图惯例）
    double width  = 420.0;       // 水平方向尺寸 (mm)
    double height = 297.0;       // 垂直方向尺寸 (mm)
    ProjectionMethod projection = ProjectionMethod::FirstAngle;  // 投影法（GB/T 14665-2012 第7章）
    QString formatName() const;  // 如 "A3 (420×297)"
    static SheetSpec make(SheetFormat f, bool landscape);
};

// GB/T 14690 比例系列判定：优先系列（1:1,1:2,1:5,1:10…；2:1,5:1,10:1…）与
// 允许系列（1:2.5,1:3,1:4,1:6,…；2.5:1,4:1…）均视为标准比例。
// ratio = 图形尺寸 / 实际尺寸（1:2 -> 0.5，2:1 -> 2.0）。
bool isStandardDrawingScale(double ratio);

// GB/T 14665-2012 5.6 / 表3「字体高度与图幅」标准字高(mm)：
//   A0/A1：汉字 7、字母数字 5；A2/A3/A4（含 A5）：汉字 5、字母数字 3.5。
// chinese=true 取汉字字高，false 取字母数字字高。
double standardTextHeightMm(SheetFormat f, bool chinese);

// 进程内「当前图纸幅面」：由「新建图纸」设定，供把「随幅面变化」的量
// （如尺寸数字字高，见 5.6 表3）派生为标准值。默认 A3。
SheetFormat currentSheetFormat();
void        setCurrentSheetFormat(SheetFormat f);

// 标题栏栏目：key 用于检索/编辑，label 为栏目标签（与生成时的 tb:label:/tb:field: 对应）
struct TitleBlockField {
    QString key;    // 如 "drawingName"
    QString label;  // 如 "图纸名称"
};

// 标题栏栏目定义（有序，供「填写标题栏」表单按固定顺序渲染）
const QVector<TitleBlockField>& titleBlockFields();

// 依据 GB/T 14689（图框）/ GB/T 10609.1（标题栏）/ ISO 128 生成图框 + 标题栏，
// 返回一组图元：
//   - 线段：layer = "图框"（图纸外边界 + 图框）/ "标题栏"（标题栏网格），粗实线框线、细实线网格；
//   - 文本：name 以 "tb:label:"（栏目标签，灰）/ "tb:field:"（可编辑栏目值，黑）开头，
//     便于「填写标题栏」表单按 key 检索并写回。
// 坐标系：图纸左下角为原点 (0,0)，X 向右、Y 向上（与模型世界坐标一致，单位 mm）。
std::vector<Element> buildSheetFrame(const SheetSpec& spec);

// 由纸张矩形尺寸(mm)反推标准幅面（容差 1mm；竖放/横放均可命中，方向由调用方按 w/h 判）。
// 用途：旧档缺顶层 "sheet" 键时，从「图框」图层的纸张外边界矩形自愈推断幅面，
// 使打印/预览/PDF 的页面方向与创建时的选择一致（详见「新建图纸幅面驱动页面设置」设计）。
// 非标尺寸（用户自画框、无框）→ nullopt：调用方必须放弃推断，**绝不猜**。
std::optional<SheetFormat> sheetFormatFromSize(double w, double h);

// 插入到「当前图纸」的标准标题栏所需字段（由调用方从目标图纸文件/当前比例等取得）
struct InsertedTitleBlock {
    QString drawingName;    // 图名
    QString drawingNumber;  // 图号
    QString scale;          // 比例（已格式化，如 "1:1" / "1:2"）
    QString designDate;     // 绘制日期（如 "2026-09-13"）
};

// 在当前图纸已有图元内容之外，绘制标准化「图纸边框 + 标准标题栏」。
//   - 标题栏位于图框右下角，含图名/图号/比例/绘制日期等必要字段，并预留单位/设计/审核栏；
//   - 栏目文本沿用 buildSheetFrame 的命名约定（name 以 "tb:field:<key>" 开头），
//     因此可与「填写标题栏」表单及 syncTitleBlockScale 无缝协作（比例栏会被自动同步）；
//   - 纯函数：仅依据 contentBBox 与 data 生成一组新图元，绝不读取/改动模型，
//     调用方应 beginEdit + addElementsQuiet(out)，从而「只追加、不修改/移动/删除已有图元」。
// 坐标系与 buildSheetFrame 一致（X 向右、Y 向上，单位 mm）。
std::vector<Element> buildInsertedTitleBlock(const QRectF& contentBBox, const InsertedTitleBlock& data);

// 依据【所选图纸幅面】与【当前内容包围盒】自动生成「图纸边框 + 标准标题栏」。
//   - 图纸边框尺寸 = max(所选幅面 spec，内容最小外接矩形 + 边距)：
//       至少达到所选幅面（A0~A5）；若已有图元超出该幅面，则把边框扩大到足以
//       把全部图元尽可能包含在内（contentBBox 外留统一安全边距）；
//   - 标题栏固定在图框右下角，并通过「自动避让」保证不与任何已有图元重叠：
//       若右下角标题栏区域与内容包围盒相交，则把图框向「代价更小」的一侧（右或下）
//       外扩，使标题栏整体位于内容右侧或下方，从而互不相交；
//   - 边框与标题栏均为「只追加」的新图元（图层：图框 / 标题栏），纯函数，
//     绝不读取/改动模型；调用方应 beginEdit + addElementsQuiet(out)。
// 坐标系与 buildSheetFrame 一致（X 向右、Y 向上，单位 mm）。
std::vector<Element> buildAutoFrameTitleBlock(const QRectF& contentBBox,
                                              const SheetSpec& spec,
                                              const InsertedTitleBlock& data);

// ===================== 【图纸】>【新建图框】图框生成选项 =====================
// 仅承载数据（只增不改）：由 SheetFrameDialog 填充，buildSheetFrameWithOptions 消费。
// 覆盖需求 1~9：基本/加长/自定义幅面、横竖布置、绘图比例(对话框双下拉协同)、
// 分区(偶数+25~75mm)、装订线开关、对中符号开关、标题栏/明细栏(下拉+开关+预览)。
struct SheetFrameOptions {
    enum class SizeMode { Basic, Extended, Custom };
    SizeMode sizeMode = SizeMode::Basic;

    SheetFormat basicFormat = SheetFormat::A3;   // Basic
    bool        landscape   = true;              // Basic / Extended 横置/竖置

    // 加长幅面（Extended）：基础格式短边 × 整数倍（GB/T 14689 第二/第三选择）
    SheetFormat extBase  = SheetFormat::A3;
    int         extFactor = 3;

    // 自定义（Custom）：直接毫米尺寸
    double customW = 420.0, customH = 297.0;

    // 装订线：true=留装订边(a=25, 其余 c)；false=不留装订边(四边 e)（需求 7）
    bool bindingMargin = true;

    // 对中符号（GB/T 14689 6.1）：图纸四边中点粗短线（需求 8）
    bool centerMarks = false;

    // 分区：长度(X)/宽度(Y)方向分区数（须为偶数；每分区 25~75mm）（需求 6）
    bool zonesEnabled = false;
    int  zoneCountX = 4;
    int  zoneCountY = 2;

    // 标题栏样式（Standard=标准 GB/T 10609.1，None=仅图框）（需求 9）
    TitleBlockStyle titleBlock = TitleBlockStyle::Standard;

    // 明细栏（GB/T 10609.2）：下拉选择系统已定义明细表 + 插入开关（需求 9）
    enum class BomStyle { Standard };
    bool bomEnabled = false;
    BomStyle bomStyle = BomStyle::Standard;
    int  bomRows = 4;   // 数据行数（不含表头）

    // ---- 以下为【新建图框】扩展选项（只增不改；默认值保持旧行为完全不变）----

    // 装订宽度：留装订边时的 a 值（默认 25 与旧行为一致；附图左下「装订宽度」输入框）
    double bindingWidth = 25.0;

    // 自动填写日期：落档时把当天日期写入标题栏「设计日期」栏（附图左下）
    bool autoFillDate = false;

    // 指定图框位置（附图左下）：预留开关。当前图框恒绘制于原点 (0,0)；
    // 该标志随模板持久化，供未来「按指定位置放置图框」功能消费（见设计文档）。
    bool specifyFramePos = false;

    // 附加栏（需求 10）：标题栏/明细栏上方堆叠；系统定义预设见 additionalBarPresets()
    bool    additionalBarEnabled = false;
    QString additionalBarKey;      // 预设 key（additionalBarPresets 中检索）

    // 代号栏（需求 11）：图框左上角；字段与标题栏「图号」共用 tb:field:drawingNumber，
    // 由 TitleBlockForm 同名全写回实现自动关联；系统预设见 codeBarPresets()
    bool    codeBarEnabled = false;
    QString codeBarKey;             // 预设 key（codeBarPresets 中检索）

    // 参数栏（需求 12）：附加栏上方堆叠；系统预设见 paramBarPresets()
    bool    paramBarEnabled = false;
    QString paramBarKey;            // 预设 key（paramBarPresets 中检索）

    // 分区子选项：是否显示左分区（左带字母 A..n，附图「是否显示左分区」子复选框）。
    // false 时仅绘制顶带数字分区；顶带不受此开关影响。
    bool zoneShowLeft = true;

    // 比例调整设置（附图右下组）：随图框比例联动三开关 + 两个子选项。
    // 「指定初始高度」生效时，图框内文字以该字高(mm)为基准；其余为随图/模板持久化
    // 的设置项（完整标注管线联动见设计文档「已知限制」）。
    bool   dimScaleAdjust       = true;   // 标注随图框比例调整
    bool   textScaleAdjust      = true;   // 文字随图框比例调整
    bool   textInitHeightEnabled = false; // 指定初始高度（子开关）
    double textInitHeight       = 3.5;    // 指定初始高度值 (mm)
    bool   lineWidthScaleAdjust = true;   // 线宽随图框比例调整
    bool   lineWidthInitEnabled = false;  // 指定初始比例（子开关）
    double lineWidthInitRatio   = 0.3;    // 指定初始比例值

    // 投影法（GB/T 14665-2012 第7章）
    ProjectionMethod projection = ProjectionMethod::FirstAngle;

    // ---- 派生 ----
    double widthMm()  const;                 // 最终水平尺寸
    double heightMm() const;                 // 最终垂直尺寸
    SheetFormat frameMarginFormat() const;   // 判定图框周边(c/e)的基本幅面（加长=大一号）
    QString sizeLabel() const;               // 展示串，如 "A3 (420×297)" / "A3×3 (420×891)"
    SheetSpec toSheetSpec() const;           // 供 MainWindow 落档/设幅面
};

// 加长幅面预设（GB/T 14689 第二选择 + 第三选择），供下拉框展示与拾取。
struct ExtendedSheetPreset {
    QString      label;     // "A3×3"
    SheetFormat base;       // A3
    int         factor;     // 3
    double      longSide;   // 891（加长后长边 = 基础幅面短边 × 倍数）
    double      shortSide;  // 420（加长后短边 = 基础幅面长边）
};
const QVector<ExtendedSheetPreset>& extendedSheetPresets();

// ---- 【新建图框】系统定义栏预设（需求 10/11/12；数量不限，注册表只增不改）----

// 附加栏预设：置于标题栏/明细栏上方，columns 为表头列（空数据行供填写）
struct AdditionalBarPreset {
    QString     key;      // 稳定标识（模板持久化用）
    QString     label;    // 下拉展示名
    QStringList columns;  // 表头列标签
    int         rows = 2; // 空数据行数
};

// 代号栏预设：置于图框左上角（上周边带内）；fieldKey 为关联的标题栏栏目 key
// （生成时字段元素名 = "tb:field:<fieldKey>"，与标题栏同名字段自动关联写回）
struct CodeBarPreset {
    QString key;       // 稳定标识
    QString label;     // 下拉展示名
    QString caption;   // 栏内标签文字（如 "图样代号"）
    QString fieldKey;  // 关联标题栏栏目（如 "drawingNumber"）
};

// 参数栏预设：置于附加栏上方（无附加栏则直接在明细栏/标题栏上方），columns 为参数表头
struct ParamBarPreset {
    QString     key;
    QString     label;    // 下拉展示名（如 "直齿锥齿轮参数表"）
    QStringList columns;  // 参数列表头
    int         rows = 3; // 空数据行数
};

const QVector<AdditionalBarPreset>& additionalBarPresets();
const QVector<CodeBarPreset>&       codeBarPresets();
const QVector<ParamBarPreset>&      paramBarPresets();

// 依据 GB/T 14689 生成「图框 + 选项叠加」（装订/对中/分区/明细栏/标题栏），纯函数。
// 默认构造的 SheetFrameOptions 与旧 buildSheetFrame(spec) 行为完全一致（向后兼容）。
std::vector<Element> buildSheetFrameWithOptions(const SheetFrameOptions& opts);

} // namespace SketchCAD
