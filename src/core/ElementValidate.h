#pragma once
#include <QString>
#include <vector>
#include "Element.h"

namespace SketchCAD {

// ============================================================================
// 鲁棒性守卫层（Robustness Guard）
// ----------------------------------------------------------------------------
// 在读档解析完成、图元进入模型之前，对「已解析」的 Element 向量做只读校验。
// 这是本次「任何输入都不能让程序崩溃/卡死/写坏文件」任务的唯一新增核心代码集中点，
// 不动任何几何算法、不改动正常文件的解析结果。
//
// 设计铁律：
//   * 正常文件：filterElements 返回向量与原向量逐元素【值等价】，skip=fix=0，绝不改动任何正常图元；
//   * 损坏输入：只做「丢弃(skip)」或「裁剪修复(fix)」，降级明细写入 report，绝不静默画错；
//   * 判定完全基于字段值的范围 / 有限性 / 退化性，不重算任何形状、不依赖渲染。
//
// 处理策略优先级（与任务要求一致）：不崩溃 > 报错明确 > 部分降级 > 完全拒绝。
// ============================================================================

// 单个图元是否可安全进入模型。
// 正常图元恒返回 true；返回 false 时 why 给出中文原因（用于日志 / 自测断言）。
bool isElementValid(const Element& e, QString* why = nullptr);

// 批量守卫：返回保留图元向量。
//   skipCount = 因损坏被丢弃的图元数；
//   fixCount  = 被裁剪修复（去重复末点 / 剥离控制字符）的图元数；
//   report    = 人类可读的中文降级摘要（每条降级一行，便于在打开日志中如实告知）。
// 正常文件：返回值与输入逐元素值等价（skip=fix=0，未做任何修改）。
std::vector<Element> filterElements(const std::vector<Element>& in,
                                   int* skipCount, int* fixCount, QString* report);

// 载入各表后统计「引用不存在」的图元（按名引用层/块/样式；按 id 引用 refA/refB/hatchSrcIds）。
// 不丢弃任何图元，仅统计并在 report 写明，便于在打开日志中如实告知用户降级。
//   elems        : 解析得到的图元向量（file-id 空间，尚未被模型重分配 id）；
//   layerNames / blockNames / styleNames : 文件中声明的各表名称集合
//                 （由调用方从模型 / 文件提取；务必包含保留层 "0"）。
//   report       : 输出（中文摘要）。
// 返回：存在缺失引用的图元数量。
int countDanglingRefs(const std::vector<Element>& elems,
                     const std::vector<QString>& layerNames,
                     const std::vector<QString>& blockNames,
                     const std::vector<QString>& styleNames,
                     QString* report);

} // namespace SketchCAD
