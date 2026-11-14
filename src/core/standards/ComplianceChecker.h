#ifndef COMPLIANCE_CHECKER_H
#define COMPLIANCE_CHECKER_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

namespace SketchCAD {

class SketchModel;   // 前向声明，避免头文件重依赖 SketchModel.h

// 工程制图合规性问题（结构化，供命令 / 面板 / 单测共用）。
// 字段与设计需求一致：elementId / ruleId / severity / message / fixHint。
struct ComplianceIssue {
    long    elementId = 0;   // 触发问题的图元 id（图层级问题为 0，或该层首个图元 id）
    QString ruleId;          // 规则编号，如 "R1_line_width_ratio"
    QString severity;        // 严重度：严重 / 警告 / 提示
    QString message;         // 问题描述
    QString fixHint;         // 修正建议
};

// 工程制图合规性检查器：覆盖 7 类 GB/T 制图规范检查。
//   R1 线宽粗细比（2:1，GB/T 14665 表1）
//   R2 两线最小间隙 ≥ 0.7mm（GB/T 14665）
//   R3 字号下限（A2~A4 字母数字 ≥ 3.5mm，GB/T 14665 表3）
//   R4 粗糙度符号书写规范（须注写 Ra 数值，GB/T 131-2006）
//   R5 焊缝符号完整性（基准线 + 基本符号，GB/T 324）
//   R6 幅面留边（内容不得超出图框留边区，GB/T 14689）
//   R7 图层命名（GB/T 18617 / ISO 13567）
//
// 设计约束：
//   * 纯查询、零副作用；不触碰 DiagBundle 诊断管线（DiagBundle 是崩溃诊断 ZIP 打包器，
//     非逐条问题上报通道）。问题上报复用 SketchModel::logAppended 信号（经 m->log）。
//   * 检查为对模型的轻量同步遍历（图元量级小、毫秒级），不另开线程即可保证不阻塞编辑器交互。
//   * 命令 → 面板刷新经全局回调桥 notifyComplianceReport，避免改动被广泛包含的 SketchModel.h
//     触发全量重编。
class ComplianceChecker {
public:
    // 检查整个模型，返回全部不合规项（按 elementId 聚合）。无问题返回空列表。
    static QVector<ComplianceIssue> run(const SketchModel* model);

    // 仅图层命名合规检查（供命令 / 面板 / 单测共用同一判定逻辑）。
    static QVector<ComplianceIssue> checkLayerNaming(const QStringList& layerNames);

    // ---- 命令 → 面板刷新回调桥（全局单例 sink，MainWindow 构造期注册）----
    using ComplianceReportSink = std::function<void(const QVector<ComplianceIssue>&)>;
    static void setComplianceReportSink(ComplianceReportSink sink);
    static void notifyComplianceReport(const QVector<ComplianceIssue>& issues);

private:
    ComplianceChecker() = delete;   // 纯静态工具类
};

} // namespace SketchCAD

#endif // COMPLIANCE_CHECKER_H
