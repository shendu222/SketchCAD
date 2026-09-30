#pragma once
#include <vector>
#include "Vec.h"
#include "Element.h"

// 直线绘制时的「智能捕捉」几何层：把「鼠标附近有哪些可吸附的几何特征点」这件事
// 完全算清楚，UI 只负责把吸附点画出来、把拾取点替换成吸附点。
//
// 捕捉类型（对应需求，在原有 Endpoint/Center/Intersection/Tangent/Midpoint 基础上扩展）：
//   Endpoint      直线/弧/矩形/多边形/样条的端点或顶点
//   Center        圆/弧的圆心
//   Intersection  两图元的交点（直线×直线、直线×圆、圆×圆……）
//   Tangent       由已知另一端点向圆(弧)作切线的切点
//   Midpoint      线段/折边中点（已有：单段中点）
//   ---- 本次新增 ----
//   Quadrant      圆/弧的象限点（0°/90°/180°/270°，即半径手柄四方位）
//   Perpendicular 由光标向图元作垂线的垂足
//   Insertion     块引用/文字/多行文字/标注的插入点
//   Node          点(Point)图元的节点位置
//   Nearest       图元上离光标最近的点
//   Apparent      两图元「外观交点」：其无限延长线/圆相交但线段本身不相交处（平面视图投影相交）
//   Parallel      与某参照图元平行的方位（用于绘制平行线；结果为沿该方位投影到已置首点的点 + 方向）
//   Extension     直线/弧端点沿其方向延长线上的点
//   MidBetween    两点间中点（需两个参考点 a、b）
// 所有计算纯几何、无 UI 依赖，可被视图、命令行与无界面自测共用。
namespace SketchCAD {
namespace snap {

enum class SnapType {
    None, Endpoint, Center, Intersection, Tangent, Midpoint,
    Quadrant, Perpendicular, Insertion, Node, Nearest,
    Apparent, Parallel, Extension, MidBetween
};

struct SnapResult {
    Vec3 point = Vec3(0, 0, 0);
    bool snapped = false;
    SnapType type = SnapType::None;
    int elemA = -1;   // 主图元（切点/圆心/端点/垂足所属图元，或交点的图元A，平行/延伸的参照图元）
    int elemB = -1;   // 次图元（交点图元B、MidBetween 的第二参考点来源图元等）
    Vec3 dir = Vec3(0, 0, 0);   // Parallel：被吸附方向（单位向量，Z=0）；其它类型为零
};

// 捕捉开关配置：绘制命令在「命令执行期间」据此决定启用哪些捕捉类型。
// 各绘制命令可持有独立的一份（由 GraphicsView 在进出命令时备份/恢复），
// 从而支持「一键切换、多模式共存、退出命令自动复原」。
struct SnapConfig {
    bool endpoint     = true;   // 端点 / 顶点
    bool center       = true;   // 圆心
    bool intersection = true;   // 两图元交点
    bool tangent      = true;   // 切点（需已知另一端点）
    bool midpoint     = false;  // 直线段中点（默认关闭，按 M 开启）

    // ---- 本次新增 ----
    bool quadrant     = false;  // 象限点（默认关）
    bool perpendicular= false;  // 垂足（默认关：与中点一致为可切换模式；经 Shift+右键/P 键开启，命中时显示 ⊥ 高亮）
    bool insertion    = false;  // 插入点（默认关）
    bool node         = false;  // 节点（点图元，默认关）
    bool nearest      = false;  // 最近点（默认关，兜底）
    bool apparent     = false;  // 外观交点（默认关）
    bool parallel     = false;  // 平行（默认关）
    bool extension    = false;  // 延伸（默认关）
    bool midBetween   = false;  // 两点间中点（默认关）

    // 序列化 / JSON 往返
    QJsonObject toJson() const;
    static SnapConfig fromJson(const QJsonObject& o);   // 缺键回落默认（全部 off 由 SnapConfig{} 决定）

    // 全等比较（setSnapConfig 幂等判定用）
    bool operator==(const SnapConfig& o) const {
        return endpoint == o.endpoint && center == o.center && intersection == o.intersection &&
               tangent == o.tangent && midpoint == o.midpoint && quadrant == o.quadrant &&
               perpendicular == o.perpendicular && insertion == o.insertion && node == o.node &&
               nearest == o.nearest && apparent == o.apparent && parallel == o.parallel &&
               extension == o.extension && midBetween == o.midBetween;
    }
    bool operator!=(const SnapConfig& o) const { return !(*this == o); }
};

// 捕捉字段定义表（key=快捷键，label=显示名，field=对应 SnapConfig 开关成员，type=对应捕捉类型）。
// 捕捉切换快捷键、Shift+右键临时覆盖菜单、捕捉设置面板、命中提示标签四处共用，
// 避免散落重复定义导致「菜单叫端点、提示叫别的名」这类漂移。
struct SnapFieldDef {
    int key;                   // Qt::Key
    QString label;             // 显示名（含快捷键提示）
    const char* jsonKey;       // JSON 序列化键名（稳定英文，与字段名一致）
    bool SnapConfig::* field;  // 对应 SnapConfig 开关成员
    SnapType type = SnapType::None;  // 该开关对应的捕捉类型（供命中提示反查显示名）
};
std::vector<SnapFieldDef> snapFieldDefs();

// 捕捉类型的显示名（含快捷键提示），供命中提示标签/日志使用。
// 由 snapFieldDefs() 反查得到，与菜单、面板、快捷键完全同源。
QString snapTypeLabel(SnapType t);

// 捕捉优先级（数值越大越优先）。当多个类型同时落入容差范围时，先按优先级，
// 同优先级再比距离。规则：显式「点状」捕捉（端/心/节/插/象）最高；然后是严格
// 几何交点；其后是「需推导」的垂足/切点；最后是「兜底/推断」的最近/外观/延伸/平行/中点。
// 这让「光标压在端点上」时优先吸端点而非顺带的最近点，符合 AutoCAD 的语义直觉。
int snapPriority(SnapType t);

// 捕捉类型统一注册表：每种捕捉具备唯一标识、显示名、适用图元范围、优先级与吸附说明。
// 与 snapFieldDefs() 互补——此处侧重「类型语义与优先级」的集中登记，供帮助/设置面板/自测查阅。
struct SnapDescriptor {
    SnapType     type;
    const char*  id;          // 机器标识（英文短名）
    const char*  name;        // 显示名（中文）
    const char*  applicable;  // 适用图元 / 触发条件
    int          priority;    // 吸附优先级（来自 snapPriority）
    const char*  desc;        // 吸附判定说明
};
std::vector<SnapDescriptor> snapTypeDescriptors();
const SnapDescriptor* snapTypeDescriptor(SnapType t);

// 图元上「真实几何意义」的捕捉点（不含圆的半径手柄这类辅助点）。
// 圆：仅圆心；弧：圆心 + 起/终点；直线：两端点；矩形：四角；多边形/样条：全部顶点。
std::vector<Vec3> snapHandles(const Element& e);

// 由外部点 P 向圆 (C, r) 作切线，返回切点个数 0/1/2。
// P 在圆内（d<r）无实切线，返回 0；P 在圆上返回 1（切点即 P 自身投影）。
// 切点满足：|T-C|=r 且 PT ⟂ CT。
int tangentPointsFromPointToCircle(const Vec3& P, const Vec3& C, double r, Vec3 out[2]);

// 圆/弧的象限点（0/90/180/270° 四个方位）。arcOnly 时只返回落在弧角范围内的那些。
std::vector<Vec3> quadrantPoints(const Vec3& center, double r, double startDeg, double endDeg, bool isArc);

// 由光标 w 向「图元」作垂线的垂足（图元为线段时落在线段内；为圆/弧时落在其上）。
// 返回是否命中及垂足点。
bool perpendicularFoot(const Vec3& w, const Element& e, Vec3& foot);

// 综合捕捉：在 tol 范围内寻找「按优先级 + 距离」最佳吸附点。
// otherEnd 非空时，额外计算「由 otherEnd 向各圆/弧作切线的切点」与「平行投影点」参与竞争。
// refA/refB 非空时（且 cfg.midBetween 开启），额外把两参考点中点纳入竞争（两点间中点）。
// cfg 决定哪些捕捉类型参与竞争（按快捷键切换的开关）。
// 返回最近命中；若 tol 内无任何吸附点，snapped=false 且 point==w（即自由点）。
SnapResult snap(const std::vector<Element>& elems, const Vec3& w,
               const Vec3* otherEnd = nullptr, double tol = 1.0,
               const SnapConfig& cfg = SnapConfig{},
               const Vec3* refA = nullptr, const Vec3* refB = nullptr);

// ======================== 追踪系统（极轴 / 对象捕捉追踪 / 临时追踪点）========================
// 三者共用同一纯几何层 track()，由 GraphicsView 在绘制命令期驱动，无 UI 依赖。
//   Polar       极轴追踪：从「已放置的基点」沿一组极轴角方向发出射线，光标落入某条射线邻域即
//                被吸附到该射线（保持当前光标到基点的距离）。
//   ObjectTrack 对象捕捉追踪：把当前 OSNAP 命中的点「获取」为对齐锚点，从其发出水平/垂直对齐线，
//                光标沿对齐线、或两锚点对齐线交点处被吸附。
//   TempTK      临时追踪点：用户显式放置的瞬态点，参与对齐线计算（与 ObjectTrack 同机制）。
enum class TrackMode { None, Polar, ObjectTrack, TempTK };

struct TrackResult {
    Vec3 point = Vec3(0, 0, 0);   // 命中的追踪落点
    bool hit = false;             // 是否命中任一种追踪
    TrackMode mode = TrackMode::None;
    Vec3 dir = Vec3(0, 0, 0);     // Polar：射线单位方向；ObjectTrack/TempTK：对齐轴方向（单轴为 (1,0)/(0,1)）
    int  axis = 0;                // ObjectTrack/TempTK：0=水平对齐, 1=垂直对齐（-1 表示 Polar）
    Vec3 anchorA, anchorB;        // 参与的锚点（ObjectTrack/TempTK 用 anchorA；Polar 用 anchorA=极轴基点）
};

// 追踪全局配置（随图存档顶层 JSON "trackConfig"）：
//   polar          极轴追踪开关
//   objectTrack    对象捕捉追踪开关（依赖 OSNAP 获取锚点）
//   polarIncrement 极轴增量角（度）：90/45/30/60/15…（用户可设）
//   polarRelative  极轴角是否相对「上一段方向」偏移（否则用绝对 0/90/180/270）
struct TrackConfig {
    bool   polar = true;
    bool   objectTrack = true;
    double polarIncrement = 90.0;
    bool   polarRelative = true;

    QJsonObject toJson() const;
    static TrackConfig fromJson(const QJsonObject& o);   // 缺键回落默认
    bool operator==(const TrackConfig& o) const {
        return polar == o.polar && objectTrack == o.objectTrack &&
               std::fabs(polarIncrement - o.polarIncrement) < 1e-9 &&
               polarRelative == o.polarRelative;
    }
    bool operator!=(const TrackConfig& o) const { return !(*this == o); }
};

// 根据配置生成极轴射线方向角（度，[0,360)）。baseDir 非空且 polarRelative 时，以 baseDir 的
// 世界角为基准叠加 0,inc,2inc…；否则从绝对 0° 起。始终额外补齐正交 0/90/180/270 作为兜底引导。
std::vector<double> polarAngles(const TrackConfig& cfg, const Vec3* baseDir);

// 综合追踪：给定光标 w、配置 cfg、上一段方向 lastSegDir（可空）、极轴基点 polarBase（可空）、
// 临时追踪点 trackPoints、已获取的对象追踪锚点 osnapAnchors、世界容差 tol，返回最佳追踪落点。
// 命中规则（按到光标距离取最优）：
//   - Polar：对每个极轴角，若 (w-polarBase) 在该角射线邻域（垂距 ≤ tol）且在前方，吸附到射线上；
//   - OTRACK/TK：对每个锚点发出水平/垂直对齐线，光标落入邻域即吸附；两锚点正交对齐线交点亦参与。
TrackResult track(const Vec3& w, const TrackConfig& cfg,
                  const Vec3* lastSegDir, const Vec3* polarBase,
                  const std::vector<Vec3>& trackPoints,
                  const std::vector<Vec3>& osnapAnchors, double tol);

// ======================== 输入约束（正交 ORTHO / 栅格吸附 SNAP）========================
// 与捕捉/追踪不同：正交与栅格吸附是**强制性输入约束**，优先级高于 OSNAP——
// 开启正交后即便 OSNAP 命中了斜向点，落点也要被投影回水平/垂直方向（画不出斜线）。
struct InputResult {
    Vec3 point = Vec3(0, 0, 0);
    bool ortho = false;   // 本次是否施加了正交方向锁定
    bool grid  = false;   // 本次是否施加了栅格量化
};

// 合成输入约束。base 为「上一段终点/已放置点」（正交基准），可为空（首点无基准 → 正交不生效）。
// 行为规则（三者互斥关系明确定义，避免「同时开启时行为异常」）：
//   ① 仅正交：保留 (p-base) 中绝对值较大的分量，另一分量归零 → 锁在过 base 的水平/垂直线上；
//   ② 仅栅格：x/y 各自量化到 step 的最近整数倍（无 base 要求）；
//   ③ 两者同时开：**正交定方向、栅格定长度** —— 先按 ① 锁轴，再把「被保留的那个分量」
//      量化到 step，另一分量保持 base 原值（保证点始终精确落在锁定轴上，不会因量化而偏出轴线）。
//      这是 AutoCAD 的语义，也消除了「正交把点锁到 x=37.6、栅格又把它拽到 x=38」导致的抖动。
//   ④ 都不开：原样返回。
InputResult applyInputConstraints(const Vec3& p, const Vec3* base,
                                  bool ortho, bool grid, double step);

} // namespace snap
} // namespace SketchCAD
