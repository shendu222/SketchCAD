#pragma once
#include <vector>
#include "Vec.h"
#include "Element.h"
#include "Geometry.h"
#include "Modify.h"   // modify::Change（剖面填充以 Change 形式产出，供 applyChanges 落盘）
#include "HatchLibrary.h"  // PAT 填充图案文件（AutoCAD .pat）解析与加载

namespace SketchCAD {

class SketchModel;

// ===================== 剖面区域表示（GB/T 4457.5-2013）=====================
//
// 本标准规定剖视图/断面图中「剖面区域」的表达方式：
//   * 4.1  同一金属零件：剖面线为间隔相等、方向相同、与主要轮廓/对称线成 45° 的细实线平行线；
//          必要时也可画成与主要轮廓线成适当角度。
//   * 4.2  相邻辅助零件（夹具/胎具）：不画剖面符号。
//   * 4.3  剖面区域较大：可只沿轮廓周边画剖面符号。
//   * 4.4  仅画被剖部分且无断裂边界：剖面线绘制整齐。
//   * 4.5  零件图中可用涂色/点阵代替剖面符号。
//   * 4.6  木材/玻璃/液体/叠钢片/砂轮/硬质合金刀片等可在外形视图画材料标志。
//   * 4.7  装配图中相邻金属零件剖面线方向应相反（或同向而间隔不等）；
//          同一零件方向相同、间隔相等；相邻同符号非金属零件用疏密不一区分。
//
// 此外，业界通行的「豁免画剖面线特殊情形」亦在此落实：
//   实心轴/杆/球/键/销纵向剖切按不剖、肋/轮辐/薄壁过对称面不画剖面线、
//   标准件（螺栓/螺母/垫圈/轴承/弹簧）不画、狭小区域（如 <2mm 孔）涂黑代替。

// 材料类别（决定剖面符号；通用金属走 45° 细实线）
enum class SectionMaterial {
    General   = 0,   // 通用金属：45° 细实线
    CastIron  = 1,   // 铸铁：网状剖面线
    Steel     = 2,   // 钢：同通用
    Wood      = 3,   // 木材：年轮符号
    Glass     = 4,   // 玻璃：特定符号
    Liquid    = 5,   // 液体：液面细实线
    Concrete  = 6,   // 混凝土
    NonMetal  = 7,   // 非金属：交叉网格线（4.7 相邻同符号非金属用疏密区分）
    Aluminum  = 8,   // 铝（非铁金属）：与钢反向 45° 单族（GB 4.7 相邻金属反向/异距区分）
    Brick     = 9,   // 砖墙：水平砌缝 + 错缝竖线（砌筑图案）
    Stone     = 10,  // 石材墙面：错缝块石（不规则砌缝，近似毛石）
    Insulation= 11,  // 保温层：点阵符号
    Plywood   = 12,  // 胶合板：交叉层（水平 + 垂直细线，示意多层胶合）
    // —— 以下为 GB/T 4457.5-2013 表1「剖面符号」其余材料（2026-09-22 扩展，追加式，保留原序号）——
    CoilWinding = 13,  // 线圈绕组元件：45° 细实线 + 点阵（GB 表1）
    LaminatedSteel = 14,  // 叠钢片：薄 45° 细实线（GB 表1）
    Sand = 15,  // 型砂/填砂/粉末冶金/砂轮/陶瓷刀片/硬质合金刀片等：自由分布点阵（GB 表1）
    Soil = 16,  // 基础周围的泥土：稀疏点阵（GB 表1）
    ReinforcedConcrete = 17,  // 钢筋混凝土：45° 线 + 点阵（钢筋示意，GB 表1）
    Mesh = 18   // 筛网/过滤网等：交叉网格（GB 表1）
};
// 材料类别总数（含 0），供 UI 循环与测试遍历上界使用。
static constexpr int kSectionMaterialCount = 19;

// 豁免类别（不画或特殊处理剖面线）
enum class SectionExempt {
    None = 0,                 // 正常画剖面线
    AuxiliaryPart = 1,        // 4.2 相邻辅助零件（夹具/胎具）：不画剖面符号
    SolidBarLongitudinal = 2, // 实心轴/杆/球/键/销 纵向剖切：按不剖（不画剖面线）
    RibSpoke = 3,             // 肋/轮辐/薄壁 过对称面剖切：不画剖面线
    StandardPart = 4,         // 标准件（螺栓/螺母/垫圈/轴承/弹簧）：不画剖面线
    SmallAreaSolid = 5        // 狭小区域（如直径 <2mm 孔）：涂黑代替剖面线
};

// 最终绘制方式
enum class HatchDraw { Hatch, SolidFill, NoFill };

struct SectionStyle {
    double angleDeg = 45.0;       // 剖面线方向（与主要轮廓/对称线成 45°，见 4.1）
    double spacingMm = 3.0;       // 剖面线间距（GB：2~4mm）
    double thinWidthPx = 0.6;     // 细实线线宽（GB/T 4457.4 细线，cosmetic 像素）
    SectionMaterial material = SectionMaterial::General;
    SectionExempt   exempt   = SectionExempt::None;
};

struct SectionRegion {
    std::vector<Vec3> boundary;             // 外边界（闭合多边形，世界坐标）
    std::vector<std::vector<Vec3>> holes;   // 内孔（布尔减）
    SectionStyle style;
};

// 把「自身闭合」的图元（圆/椭圆/矩形/闭合多边形/闭合折线/Hatch，以及首尾自
// 重合的样条或整圆圆弧）转换成剖面区域的边界点列（世界坐标，未重复首尾）。
// 支持带圆角/弧段的折线（按弧自适应采样）。
// 返回 false 的情形（不构成闭合区域）：直线、开放折线、开放样条、非整圆圆弧、
// 标注/文本/约束等。判据 = "自身几何是否闭合"，不再以点数多寡冒充闭合。
bool sectionOutline(const Element& e, std::vector<Vec3>& out);

// 端点拼接容差默认值(mm)：两端点相距不超过该值即视为可连接/已闭合。
// 0.5mm 相当于 1:1 图纸上约 0.5mm 的作图误差，兼顾"捕捉对齐"与"真实缝隙"。
constexpr double kDefaultClosureTol = 0.5;

// 闭合性诊断统计（供 UI 精确提示"成功几个 / 跳过几个 / 为什么跳过"）。
struct SectionClosureStats {
    int selfClosedRegions = 0;   // 由单个自身闭合图元形成的区域数
    int chainedRegions    = 0;   // 由多个图元首尾相接成环形成的区域数
    int openChains        = 0;   // 参与链式匹配但最终未能闭合的段链数
    int openElements      = 0;   // 完全无法成链的孤立开放图元数（直线/弧/样条…）
    int degenerate        = 0;   // 退化（零面积 / 点太少）被安全跳过的区域数
};

// 把一组图元解析为「真实闭合」的边界点列集合（世界坐标，未重复首尾）。
//   * 自身闭合图元（圆/椭圆/矩形/闭合多边形/闭合折线/闭合样条/Hatch）→ 各自成为一个区域；
//   * 其余图元（直线 / 圆弧 / 开放折线 / 开放样条）按其几何拆成"段"，再按端点容差 tol
//     首尾相接串成环；只有首尾距离 ≤ tol 的链才算闭合、才成为区域；
//   * 非闭合的链 / 孤立开放图元被安全跳过（计入 stats，不产生区域）。
// tol：端点拼接容差(mm)；stats 可空。返回的区域数即"可填充的闭合区域数"。
std::vector<std::vector<Vec3>> buildClosedLoopsFromElements(
    const std::vector<Element>& elems, double tol = kDefaultClosureTol,
    SectionClosureStats* stats = nullptr);

// 由 Element(Hatch) 还原剖面区域：boundary=pts；holes=holes；
// 角度=rotation；间距=radius（<=0 回落默认）；豁免/材料取自专用字段。
SectionRegion hatchRegionFromElement(const Element& e);

// 剖面区域 -> Hatch 图元：边界写为多边形 pts，holes/exempt/material/角度/间距写入专用字段。
Element makeHatchElement(const SectionRegion& r, const QString& name = QStringLiteral("剖面区域"));

// 生成剖面线（裁剪到 region，含内孔 even-odd），返回世界坐标线段集合。
std::vector<geo::Seg> generateHatchLines(const SectionRegion& region);

// 由 PAT 图案定义（AutoCAD .pat，见 HatchLibrary.h）生成剖面线。
// 图案自带角度/间距/笔段，忽略 region.style 的 anglesDeg/spacingMm；返回世界坐标线段，
// 由 paintHatch 的 setClipPath 裁剪到区域（含内孔）。
std::vector<geo::Seg> generateHatchLinesFromPat(const PatPattern& pat, const SectionRegion& region);

// 剖面填充图案：由「材料类别」决定剖面线族（GB/T 4457.5-2013 及行业材料符号惯例）。
//   * 通用金属/钢：与主要轮廓成 45° 的单一族细实线（间隔相等、方向相同）；
//   * 铸铁/非金属/混凝土：45° + 135° 双向交叉（网状 / 交叉网格 / 碎石点）；
//   * 木材：顺纹水平线族（近似木纹）；
//   * 玻璃：45° 长短画线（断续线，dashes=true + dashLongShort=true）；
//   * 液体：液面以下水平短画线（断续线，dashes=true + 水平族）；
//   * 线圈绕组元件/钢筋混凝土：45° 细实线 + 点阵；型砂/粉末冶金/泥土：自由分布点阵；
//   * 筛网/过滤网：交叉网格（0° + 90°）。
// 间距沿用 region.style.spacingMm；角度族对「通用金属/钢」尊重用户设定的 angleDeg，
// 其余材料按标准固定方向（网状等仍锚定在 45° 基准上旋转）。
struct HatchPattern {
    std::vector<double> anglesDeg;   // 各斜线族方向角（度）；空表示无斜线族（纯点阵）
    double spacingMm = 3.0;          // 线族间距（mm）
    QString patName;                 // 非空=改用 hatch/ 目录下同名 PAT 图案（忽略 anglesDeg/spacingMm）
    bool   dots = false;             // 是否同时绘制点阵（线圈/型砂/泥土/钢筋混凝土/混凝土）
    double dotStepScale = 1.0;       // 点阵步长缩放（1.0=常规；>1 更稀疏，如基础周围泥土）
    // 断续线（虚线）剖面符号（GB/T 4457.5-2013 表1：玻璃=45° 长短画线；液体=液面下水平短画线）。
    // generateHatchLines 在生成斜线族后按 dash 参数把每条长线切成「实段+间隔」的断续线；
    // 非 brick 路径才生效。longShort=true → 长/短交替（玻璃）；false → 均匀短线（液面）。
    bool   dashes = false;           // 是否以断续线（虚线）绘制剖面线
    bool   dashLongShort = false;    // true=长+短交替（玻璃）；false=均匀短线（液面）
    double dashLenMm  = 8.0;         // 实段长（均匀模式）或长段长（长+短模式，mm）
    double dashShortMm = 4.0;        // 短段长（仅长+短模式，mm）
    double dashGapMm  = 4.0;         // 实段之间的间隔（mm）
    // 砌体图案（砖墙/石材）：由 generateHatchLines 的 brick 分支消费（见 Section.cpp）。
    // 线段均超出区域包围盒，实际裁剪由 GraphicsView::paintHatch 的 setClipPath 完成，
    // 故生成器只需按砌层/块宽铺满包围盒即可。
    bool   brick = false;            // 是否生成砌体图案（水平砌缝 + 竖缝）；true 时忽略 anglesDeg
    bool   brickStagger = true;      // 竖缝是否错缝（砖墙=错缝；均质块石可关）
    double brickCourseMm = 30.0;     // 砌层高度（每皮砖/石高，mm）
    double brickWidthMm  = 60.0;     // 单块宽度（错缝步距，mm）
    bool   brickIrregular = false;   // 石材：竖缝位置确定性伪随机偏移，近似毛石/块石墙面
};
HatchPattern hatchPatternFor(const SectionStyle& style);

// 生成剖面点阵（玻璃/液体/混凝土等点阵符号），返回落在区域内且避开内孔的世界坐标点集。
std::vector<Vec3> generateHatchDots(const SectionRegion& region);

// 相邻区域剖面线自动区分（GB 4.7）：相邻(边界相交)区域分配相反方向(45°/135°)，
// 若同色 clique 出现奇环无法二色，则对冲突节点改用不同间距。返回是否修改了任一区域。
bool resolveAdjacency(std::vector<SectionRegion>& areas, double baseAngle = 45.0);

// 由「选中的闭合图元」生成剖面填充动作（MainWindow 与回归测试共用，避免 UI 层重复实现）。
//   * 每个可闭合图元（圆/椭圆/矩形/多边形/闭合折线/样条/Hatch）→ 一个 Hatch 替换动作；
//   * 复用源图元 ID（走 applyChanges 的 updateElementQuiet 分支 → 单次撤销、选中态不跳）；
//   * autoDistinguish 为真且区域数 >1 时自动套用 4.7 相邻区分；
//   * 无可闭合图元时返回空列表（调用方据此提示用户）。
// 重要：applyChanges 以 Change::replacement.id 判定目标图元，故本函数必须把源 ID
//       显式写入 replacement.id —— 漏写会退化成对不存在图元的静默空更新（界面"无反应"）。
std::vector<modify::Change> makeSectionChanges(const std::vector<Element>& srcs,
                                              const SectionStyle& style,
                                              bool autoDistinguish);

// makeSectionChanges 的"带回报"版本：除动作列表外，还回报
// 「成功填充 N 个闭合区域 / 跳过 M 个及原因」，供 UI 给出精确提示。
//   * 每个闭合区域产出一个"以源图元 ID 复用"的 Hatch 替换动作（replacement.id = 源 ID）；
//   * 若一个区域由多个图元链式成环而成，则复用首个图元的 ID 承载 Hatch，
//     其余参与成环的图元追加 remove 动作（避免残留重复轮廓；动作列表因此可能 > 区域数）；
//   * closureTol：端点拼接容差(mm)，默认 0.5。
struct SectionResult {
    std::vector<modify::Change> changes;   // 替换动作 + 链式成环时的合并删除动作
    int filledRegions     = 0;             // 成功生成的剖面区域(Hatch)数
    int chainedRegions    = 0;             // 其中由多图元链式成环而来的区域数
    int skippedOpenChains = 0;             // 未闭合的段链数（跳过）
    int skippedOpenElements = 0;           // 孤立开放图元数（跳过）
    int skippedDegenerate = 0;             // 退化区域数（跳过）
    bool anySuccess() const { return filledRegions > 0; }
};
SectionResult makeSectionChangesEx(const std::vector<Element>& srcs,
                                   const SectionStyle& style,
                                   bool autoDistinguish,
                                   double closureTol = kDefaultClosureTol);

// ===================== 点选拾取（"剖面填充"的交互核心）=====================
//
// 与 makeSectionChanges 的"先选图元再填充"不同，点选模式只给出**鼠标点**：
// 程序自动解析全图的闭合边界，找出"包含该点的最内层闭环"，仅填充该闭环所围区域。
//
// 语义（保证"不填到相邻区域、不漏填"）：
//   * 命中 = 包含该点的所有闭环中**面积最小**者（最内层）。相邻区域共享边界时
//     各自独立成环，点击只会落到所在的那一个，不会互相吞并。
//   * 嵌套闭环（严格位于命中环内部的更小闭环）自动成为**内孔**（even-odd 掏空）：
//     点击"环形区域"得到「外环 − 内环」，点击内环内部得到的才是内环本身。
//   * 已有剖面区域(Hatch)本身仍参与成环（其外边界可作为相邻区域的公共边），
//     但点在其内部时回报 AlreadyFilled，不重复填充。

// 全图闭环集合（一次解析、多次点选；供 UI 缓存以避免逐帧重算）。
struct SectionLoopSet {
    std::vector<std::vector<Vec3>> loops;        // 各闭环点列（世界坐标，未重复首尾）
    std::vector<std::vector<int>>  loopSrcIds;   // 各闭环的源图元 id（按拼接顺序、去重）
    std::vector<double>            loopArea;     // 各闭环面积（绝对值）
    std::vector<int>               loopHatchId;  // 该闭环来自已有剖面区域则为该 Hatch 的 id，否则 0
    SectionClosureStats            stats;        // 解析统计（未闭合链 / 孤立开放图元 / 退化）
    bool empty() const { return loops.empty(); }
};

// 解析全图（含已有 Hatch）为闭环集合。tol：端点拼接容差(mm)。
SectionLoopSet buildSectionLoopSet(const std::vector<Element>& elems,
                                   double tol = kDefaultClosureTol);

// 点选结果 / 失败原因
enum class RegionPickReason {
    Ok = 0,        // 命中可用区域
    NoLoopAtPoint, // 点不在任何闭环内（含"全图无闭环"与"点在开放轮廓处"）
    AlreadyFilled, // 点在已有剖面区域内（避免重复填充）
    Degenerate     // 命中区域退化（净面积 ≈ 0）
};

struct RegionPickResult {
    RegionPickReason reason = RegionPickReason::NoLoopAtPoint;
    bool   found = false;                        // == (reason == Ok)
    int    loopIndex = -1;                       // 命中闭环在集合中的下标
    int    totalLoops = 0;                       // 全图闭环总数（供提示）
    std::vector<Vec3> boundary;                  // 命中区域外边界
    std::vector<std::vector<Vec3>> holes;        // 内孔（严格位于 boundary 内的嵌套闭环）
    std::vector<int> boundarySrcIds;             // 外边界源图元 id
    std::vector<int> holeSrcIds;                 // 内孔源图元 id
    std::vector<int> exclusiveSrcIds;            // 命中区域"独占"的边界源图元 id
                                                 // （不与任何其它闭环共享，且本环非内孔环）
                                                 // —— 仅这些可被并入/替换为 Hatch；其余
                                                 // 边界图元是相邻区域的公共边，必须保留。
    double area = 0.0;                           // 净面积 = |外环| − Σ|内孔|

    bool ok() const { return found && reason == RegionPickReason::Ok; }
};

// 在闭环集合中拾取"包含 p 的最内层闭环"所围区域。
RegionPickResult pickRegionAt(const SectionLoopSet& set, const Vec3& p);

// 一步到位（解析 + 拾取），供测试与一次性调用。
RegionPickResult pickSectionRegionAt(const std::vector<Element>& elems, const Vec3& p,
                                     double tol = kDefaultClosureTol);

// 由拾取结果生成剖面填充动作：
//   * 仅"独占"的外边界源图元（pick.exclusiveSrcIds）会被消费：
//       第一个独占图元 → 原地替换为 Hatch（复用其 id，继承颜色/图层/线宽）；
//       其余独占图元 → 合并删除（几何已写入 Hatch，避免重复轮廓）；
//   * 与相邻区域共享的边界图元（公共边 / 作为整体外框的同一图元）一律保留：
//       否则填充本区域会把相邻区域一并裁剪掉（"另一部分被一并裁掉"缺陷）。
//       若区域内所有边界图元都与相邻区域共享，则退化为"纯新增 Hatch"，不改动任何原图元；
//   * 内孔几何写入 Hatch.holes（剖面线 even-odd 自动掏空）；
//   * 若按 style.spacingMm 一条剖面线都画不出（区域过窄），自动逐级减半
//     （下限 0.8mm，最多 3 次），避免"整块空白"式填充不完整。
// pool 用于查源图元属性；源 id 非法时退化为"纯新增 Hatch"。
SectionResult makeSectionChangeForPick(const RegionPickResult& pick,
                                       const std::vector<Element>& pool,
                                       const SectionStyle& style);

// 豁免/尺寸 -> 最终绘制方式。
//   AuxiliaryPart/SolidBarLongitudinal/RibSpoke/StandardPart → NoFill（不画剖面符号）
//   SmallAreaSolid → SolidFill（涂黑代替剖面线）
//   None → Hatch（normal）
// 注：4.7 的「疏密不一」由 resolveAdjacency 在相邻同符号非金属场景处理。
HatchDraw hatchDrawMode(const SectionRegion& region);

} // namespace SketchCAD
