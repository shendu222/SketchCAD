#pragma once
#include <QString>
#include <QVector>
#include <QRectF>
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

} // namespace SketchCAD
