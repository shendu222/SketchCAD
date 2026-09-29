#pragma once
#include <vector>
#include "Element.h"
#include "Vec.h"

namespace SketchCAD {

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
//
// 不支持的（目前自动检测不到的）：
//   Coincident/Equal/Horizontal/Vertical/Fixed - 这些通过端点重合的拓扑联动已处理
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
};

} // namespace SketchCAD
