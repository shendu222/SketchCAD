#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

class SketchModel;   // 前向声明：显式约束求解需要回写模型（实现文件里再 include）

// 约束求解器：在图元参数修改后，检测并恢复图元间的几何约束关系。
//
// 工作原理：
// 1. 修改前：扫描所有图元对，记录原先满足约束的图元对（约束关系表）
// 2. 修改后：对每对原先有约束的图元，检查约束是否被破坏
//    - 若破坏：调用 restoreConstraint 调整被动方几何以恢复约束
//    - 调整后再检查该被动方与其他图元的约束链，实现多级传播
//
// 支持的约束类型：
//   Tangent      - 直线与圆相切、圆与圆相切
//   Perpendicular - 两直线垂直
//   Parallel     - 两直线平行
//   Concentric   - 两圆(弧)同心（本次新增，可自动检测）
//
// 不支持自动检测、需显式添加的（见下方 ConstraintRecord / addConstraint）：
//   Coincident/Symmetric/Dimensional/Equal/Horizontal/Vertical/Fixed

// 显式约束记录：由用户通过命令/面板添加，存于 SketchModel 的约束表并随图存档。
// 与「自动检测的隐式约束」互补：隐式只维持既成事实，显式则主动把几何解到目标状态。
struct ConstraintRecord {
    int            id     = -1;                     // 承载该约束的图元 id（-1 表示未入模）
    ConstraintType type   = ConstraintType::Fixed;  // 约束类型
    int            idA    = -1;                     // 主动方（参照，不被移动）
    int            idB    = -1;                     // 被动方（被求解移动到满足约束）
    int            axisId = -1;                     // Symmetric/SymmetricPoint：对称轴图元（直线）id
    double         value  = 0.0;                    // Dimensional/Distance/Angle：目标值
    int            idxA   = -1;                     // 点基约束中 A 侧使用的控制点索引（默认 0；<0 表示未指定）
    int            idxB   = -1;                     // 点基约束中 B 侧使用的控制点索引（默认 0；<0 表示未指定）
    bool           satisfied = false;               // 最近一次求解结果
    // ---- 尺寸驱动 / 参数化建模关联（Request M 新增）----
    int            driverId = -1;                   // 驱动本约束的尺寸标注图元 id（-1 表示非标注驱动）
    QString        paramRef;                        // 关联的参数表变量名（如 "L"）；空串表示不关联参数
};

// 求解失败条目：带图元 id 与具体原因，便于用户定位到出问题的那两个图元。
struct SolveFailure {
    int            idA = -1, idB = -1;
    ConstraintType type = ConstraintType::Fixed;
    QString        reason;
};

// 求解报告：总数 / 成功数 / 失败明细
struct SolveReport {
    int                      total = 0;
    int                      solved = 0;
    std::vector<SolveFailure> failures;
    struct NumericDiagnostics {
        bool            ran = false;       // 是否走了数值求解路径
        bool            converged = false;  // 迭代是否收敛
        int             iterations = 0;     // 实际迭代次数
        int             rank = 0;           // 雅可比有效秩（独立约束数）
        int             dof = 0;            // 自由度 = 变量数 - 秩
        double          residualNorm = 0.0; // 收敛后残差二范数（mm）
        int             redundantCount = 0; // 冗余约束条数
        std::vector<int> redundantIdx;      // 冗余约束在约束表中的下标
        std::vector<int> infeasibleIdx;     // 求解后仍不满足（疑似冲突/过约束）的下标
    } diag;
};

// 约束统一注册描述符：每种约束具备唯一标识、显示名称、适用图元范围与参数定义。
// constraintDescriptors() 返回全部 kConstraintTypeCount 种约束的注册信息，供命令/面板/帮助查阅。
struct ConstraintDescriptor {
    ConstraintType type;      // 枚举值（即唯一标识）
    const char*    id;        // 机器标识（英文短名，命令可用）
    const char*    name;      // 显示名称（中文）
    const char*    applicable;// 适用图元范围
    const char*    params;    // 参数定义（value / axisId / idxA / idxB 的用途）
    const char*    desc;      // 一句话说明
};

// 数值求解选项
struct SolveOptions {
    int    maxIterations = 40;     // LM 最大迭代次数
    double lambda0 = 1e-3;         // 初始阻尼因子
    double tol = 1e-7;             // 残差二范数收敛阈值（mm）
    double step = 1e-6;            // 有限差分步长（mm）
    bool   computeDiagnostics = true;
};

// 逐实体约束信息：把"自由度 / 欠约束 / 过约束"定位到具体图元
struct ElementConstraintInfo {
    int id = -1;                       // 图元 id
    ElementType type = ElementType::Line;
    int naturalDof = 0;               // 该图元的固有几何自由度
    int activeVars = 0;               // 实际纳入求解器的可控自由度（被约束引用且未锁定）
    int appliedRank = 0;              // 作用于该图元的独立约束条件数（雅可比列块秩）
    int remainingDof = 0;             // 剩余自由度 = activeVars − appliedRank
    bool overconstrained = false;     // 是否参与冲突/冗余约束（过约束）
    std::vector<int> constraints;     // 涉及该图元的约束在约束表中的下标
};

// 数值求解报告（含雅可比/自由度/冗余/冲突信息）
struct SolveDiagnostics {
    bool             converged = false;
    int              iterations = 0;
    int              variables = 0;   // 决策变量数
    int              rank = 0;        // 雅可比有效秩
    int              dof = 0;         // 自由度
    double           residualNorm = 0.0;
    std::vector<int>  redundantIdx;   // 冗余约束下标
    std::vector<int>  infeasibleIdx;  // 冲突/过约束下标
    std::vector<ElementConstraintInfo> elementInfo; // 逐实体自由度/欠过约束信息
};

// 系统级约束诊断报告
struct ConstraintDiagnosis {
    enum SystemState { WellConstrained, UnderConstrained, OverConstrained, Conflicting, Mixed };
    SystemState state = WellConstrained;
    int totalDof = 0;                        // 所有几何图元的固有自由度之和
    int freeDof = 0;                         // 被约束引用图元的剩余自由度之和
    int overDof = 0;                         // 冲突/冗余约束条数
    std::vector<ElementConstraintInfo> elementInfo;
    std::vector<int> underconstrainedIds;    // 欠约束（约束不足）的图元 id
    std::vector<int> overconstrainedIds;     // 过约束/冲突的图元 id
    std::vector<QString> messages;           // 人类可读的诊断提示
    QString stateText() const;               // 系统状态的一句中文描述
};

class ConstraintSolver {
public:
    // 约束关系记录：哪两个图元之间存在什么约束
    struct ConstraintPair {
        int idA;        // 主动方（参数被修改的图元）
        int idB;        // 被动方（需要调整以恢复约束的图元）
        ConstraintType ct;
    };

    // 扫描所有图元，记录修改前满足约束的图元对
    // 返回约束关系表。调用方应在修改图元前调用此方法。
    static std::vector<ConstraintPair> detectConstraints(
        const std::vector<Element>& elems,
        const std::vector<int>& changedIds,
        double tol = 0.5);

    // 在图元参数修改后，根据约束关系表恢复被破坏的约束
    // elems: 可修改的图元列表（将被原地修改）
    // constraints: 修改前检测到的约束关系表
    // changedId: 本次修改的主动方图元 id
    // 返回被修改的图元 id 列表（不含主动方自身）
    static std::vector<int> restoreConstraints(
        std::vector<Element>& elems,
        const std::vector<ConstraintPair>& constraints,
        int changedId,
        double tol = 0.5);

    // 检测并恢复所有约束（一步到位，供简单场景使用）
    // beforeElems: 修改前的图元快照（用于检测原有约束）
    // afterElems: 修改后的图元列表（将被原地修改以恢复约束）
    // changedId: 本次修改的图元 id
    // 返回被修改的图元 id 列表
    static std::vector<int> solveAfterChange(
        const std::vector<Element>& beforeElems,
        std::vector<Element>& afterElems,
        int changedId,
        double tol = 0.5);

    // ---- 几何关系的「自动计算 + 生成」（供绘制命令/宏调用）----
    // 返回的新图元已绑定 ctype/refA/refB，使其进入本求解器的维护范围：
    // 后续移动端点时，Perpendicular/Tangent 约束会被自动维持（见 detect/restore）。

    // 绘制垂线：过 P 作一条垂直于 refLine 的直线（P 落在直线上，方向 ⟂ refLine）。
    // refLine 需为直线/线段类；返回生成的 Line 图元（ctype=Perpendicular, refA=refLine.id）。
    static Element drawPerpendicularTo(const Element& refLine, const Vec3& P);

    // 公切线：求圆/圆弧 cA 与 cB 的全部公切线（外切线 2 条 + 内切线 2 条，按存在性返回 0..4）。
    // 每条切线为一条 Line 图元（ctype=Tangent, refA=cA.id, refB=cB.id），便于后续约束维持。
    // 半径相近时仅外切线，相离时外加内切线；同心或不相交/包含时返回 0。
    static int commonTangents(const Element& cA, const Element& cB, std::vector<Element>& out);

    // 象限连接：从 from 点作一条直线连到 target 圆/弧「最近的象限点」（0/90/180/270° 方位）。
    // 返回生成的 Line 图元（refA=target.id，供后续维持）。
    static Element quadrantConnector(const Element& target, const Vec3& from);

    // ==================== 显式约束：添加 / 求解 / 冲突检测 ====================
    // 与上面「隐式自动维持」的区别：这里由用户显式声明"我要这两个图元满足某关系"，
    // 求解器负责把被动方几何解到满足为止；解不动则给出**可定位**的失败原因（含图元 id）。

    // 添加一条显式约束。成功返回空串；失败返回原因（可直接 log 给用户）。
    // 校验顺序：类型合法 → 两端图元存在 → 图元类型与该约束相容 → 登记后交给数值求解器解到满足。
    //   type=Symmetric/SymmetricPoint 时 axisId 必须是一条直线；
    //   type=Dimensional/Distance/Angle 时 value 必须 > 0；
    //   idxA/idxB 为点基约束（Coincident/PointOnLine/…）使用的控制点索引，缺省 0。
    static QString addConstraint(SketchModel& m, ConstraintType type,
                                 int idA, int idB, double value = 0.0, int axisId = -1,
                                 int idxA = -1, int idxB = -1);

    // 求解全部显式约束（迭代多轮，处理约束链传播）。返回求解报告。
    // 每轮只移动「被动方」，主动方与 Fixed 图元保持不动；某条实在解不动则记入 failures。
    static SolveReport solveAll(SketchModel& m, int maxIterations = 6);

    // 冲突/过约束检测：返回人类可读的问题列表（空 = 无冲突）。
    // 覆盖：互斥约束（平行∧垂直、水平∧竖直）、Fixed 图元被要求移动、
    //       同一对图元重复约束、Equal 与不同 Dimensional 值矛盾、同心∧相切矛盾。
    static std::vector<QString> detectConflicts(const SketchModel& m);

    // 图元固有自由度（用于逐实体 DOF 分析）
    static int naturalDof(const Element& e);
    // 逐实体欠约束/过约束诊断（只读，不修改模型）
    static ConstraintDiagnosis diagnose(const SketchModel& m);

    // 返回所有「涉及图元 id」的约束在约束表中的下标集合
    // （idA==id || idB==id || axisId==id）。供约束栏面板与冲突高亮快速定位与增删。
    static std::vector<int> constraintsInvolving(const SketchModel& m, int id);

    // ==================== 尺寸驱动 / 参数化建模（Request M 新增）====================
    // 把一条「尺寸标注(Dimension)图元」与几何体的尺寸类约束建立（或刷新）双向关联：
    //   - 依 dtype 自动生成 Dimensional(长度/半径) / Distance / Angle 约束，driverId 指回该标注；
    //   - 标注的 dimValue 作为约束目标值；修改 dimValue/参数表即重解驱动几何；
    //   - 几何被其它约束改变后，syncDimensionValues 回写 dimValue，标注文本随之更新。
    // paramRef 非空时一并登记 parameter 关联（dimParam）。返回空串=成功，否则返回原因。
    static QString linkDimension(SketchModel& m, int dimId, const QString& paramRef = QString());

    // 设定某标注的驱动值（mm 或 °）：写回 dim.dimValue 并把所有 driverId==dimId 的尺寸约束
    // 目标值设为该值，立即重解驱动几何。若尚未关联则自动 linkDimension。空串=成功。
    static QString setDimensionValue(SketchModel& m, int dimId, double value);

    // 求解后回写：把所有"已关联(dimLinked)"标注的 dimValue 同步为当前几何量，并刷新其文本。
    // 不修改几何本身。用于"几何被拖动/其它约束改变 → 标注更新"。
    static void syncDimensionValues(SketchModel& m);

    // 参数表联动：把参数名 param 设为 val，再把所有 paramRef==param 的尺寸约束（及其
    // driverId 标注）重解驱动几何。空串=成功。
    static QString driveParameter(SketchModel& m, const QString& param, double val);

    // 测量某标注当前对应的几何量（半径/直径/长度/距离/角度，按 dtype 自然单位）。
    // dimId 必须是尺寸标注图元；失败返回 NaN（用 std::isnan 判定）。
    static double measureDimension(const SketchModel& m, int dimId);

    // ==================== 数值约束求解器（雅可比 + Levenberg-Marquardt）====================
    // 面向「超过 20 种」全约束集的统一框架：把每条约束展开为残差函数，
    // 用有限差分构造雅可比，LM 迭代把被动方解到满足；支持自由度(DOF)统计、
    // 冗余检测、冲突/过约束判定。解析法(applyOne)仅作为兜底。
    static SolveDiagnostics solveAllNumeric(SketchModel& m, const SolveOptions& opt = SolveOptions{});

    // 约束统一注册表：返回全部 kConstraintTypeCount 种约束的描述符
    static std::vector<ConstraintDescriptor> constraintDescriptors();
    static const ConstraintDescriptor* constraintDescriptor(ConstraintType t);
};

// 自由变量标识：某个图元的某个可优化分量
struct VarKey {
    int elemId = -1;
    int comp = 0;   // 0 = 点 x, 1 = 点 y, 2 = 半径
    int idx = 0;    // 点索引（comp=0/1 时有效）
    bool operator==(const VarKey& o) const { return elemId==o.elemId && comp==o.comp && idx==o.idx; }
    bool operator<(const VarKey& o) const {
        if (elemId != o.elemId) return elemId < o.elemId;
        if (comp  != o.comp)  return comp  < o.comp;
        return idx < o.idx;
    }
};

} // namespace SketchCAD
