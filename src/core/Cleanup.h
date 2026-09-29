#pragma once
#include <vector>
#include <map>
#include <QString>
#include "Element.h"

namespace SketchCAD {

class SketchModel;

// ============================================================================
// 图形清理（PURGE + OVERKILL）
// ----------------------------------------------------------------------------
// 长期编辑后图面容易积累两类"浮肿"数据：
//   1) 不可见/退化的垃圾图元（零长直线、零半径圆/弧、NaN 坐标、空文字、空多边形…）
//      它们渲染不可见却占据文件体积与命中检测开销；
//   2) 重复/重叠的几何对象（完全重合或共线重叠的线段、同心同径圆、同起止角圆弧…）
//      典型来自多次复制-粘贴、导入叠加、自动追踪残留。
// 本模块提供一次性清理：保留有效实体与用户编辑内容，不破坏原有图形结构，
// 并在执行前后输出图元数量对比统计。
// ============================================================================
namespace cleanup {

struct CleanupOptions {
    bool purgeGarbage       = true;   // 清除退化/无效/不可渲染的垃圾图元
    bool purgeInvisible     = false;  // 同时清除 visible==false 的隐藏图元（谨慎，默认关）
    bool purgeUnusedBlocks  = true;   // 清除无引用的块定义（冗余数据）
    bool purgeUnusedLayers  = false;  // 清除空图层（谨慎，默认关）
    bool overkill           = true;   // 重复图元清理（OVERKILL：删除重合 / 合并重叠）
    double tol              = 1e-3;   // 几何容差(mm)
};

struct CleanupStats {
    int totalBefore = 0;
    int totalAfter  = 0;
    int garbageRemoved = 0;           // 垃圾/隐藏图元删除数
    int dupDeleted    = 0;            // 完全重合被删除的副本数
    int merged        = 0;            // 重叠合并发生次数（每次净减少 1 个图元）
    int blocksPurged  = 0;            // 无引用块定义清除数
    int layersPurged  = 0;            // 空图层清除数
    std::map<ElementType, int> removedByType;   // 按类型统计删除数（垃圾+重复+合并）
    std::vector<QString> detail;      // 人类可读的每条删除/合并说明

    QString summary() const;          // 一行对比统计："图元 N → M（移除 K：垃圾 A / 重复 B / 合并 C；冗余块 D / 空图层 E）"
};

// 在 model 上就地执行清理。
//   * 图元增删走一次 beginEdit + modify::applyChanges（可一次 Ctrl+Z 撤回）；
//   * 块/图层清除各自走自身事务（含独立撤销）。
//   * dryRun=true 时只统计、不修改模型（用于「预览统计」）。
// 返回执行前后对比统计。
CleanupStats runCleanup(SketchModel* model, const CleanupOptions& opt, bool dryRun = false);

} // namespace cleanup
} // namespace SketchCAD
