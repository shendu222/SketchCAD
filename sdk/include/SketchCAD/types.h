#pragma once

#include <cstddef>

// ============================================================================
// SketchCAD 原生 SDK —— 公共 POD 类型
// ----------------------------------------------------------------------------
// 这些类型是对「宿主内部 Element / Vec3」的稳定投影，刻意设计为
// 纯 POD（平凡构造 / 可平凡拷贝 / 无虚函数 / 无指针所有权），
// 以便安全地跨越 DLL 边界按值传递（值语义，无所有权陷阱）。
//
// 字符串一律采用 const char*（UTF-8），由实现方（宿主）持有生命周期；
// 插件只读、不释放、不修改。生命周期规则见各接口方法注释
// （通常「在下次同对象调用前保持有效」）。
// ============================================================================

namespace SketchCAD {
namespace sdk {

// 世界坐标三维点（mm）。右手系：X 向右、Y 向上、Z 朝向观察者。
// 二维图元忽略 z（恒为 0）。
struct SdkPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// 图元只读快照的关键字段（供插件枚举 / 诊断用）。
// 通过 IModel::elementInfo() 填充。其中的 const char* 指向宿主内部缓冲，
// 仅在「下一次对本模型调用读方法」前有效。
struct SdkElementInfo {
    int         id          = 0;     // 图元 id（>0）
    const char* typeName    = "";    // 类型中文名（UTF-8），如 "直线"
    const char* layer       = "";    // 图层名（UTF-8）
    int         pointCount  = 0;     // 控制点数量
};

} // namespace sdk
} // namespace SketchCAD
