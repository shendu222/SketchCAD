#pragma once

#include "types.h"

// ============================================================================
// SketchCAD 原生 SDK —— 模型（数据库）接口（对标 ObjectARX AcDbDatabase）
// ----------------------------------------------------------------------------
// IModel 是插件访问「图纸图元」的稳定入口。它是对宿主 SketchModel 的
// 受控投影：仅暴露「创建 / 查询 / 删除 / 日志」四类安全操作，
// 不暴露任何内部类型（Element / Vec3 / QObject），从而锁定 ABI。
//
// 所有方法均为纯虚；字符串参数 / 返回值为 UTF-8 const char*，由实现方
// 持有生命周期（读接口返回的指针在下一次本对象读调用前有效）。
//
// 几何约定：世界坐标，单位 mm；右手系 X 右 Y 上。角度单位为度。
// ============================================================================

namespace SketchCAD {
namespace sdk {

class IModel {
public:
    virtual ~IModel() = default;

    // ---- 图元创建：返回新图元 id（>0），失败返回 -1 ----
    virtual int addLine(double x1, double y1, double x2, double y2) = 0;
    virtual int addCircle(double cx, double cy, double r) = 0;
    virtual int addArc(double cx, double cy, double r,
                       double startDeg, double endDeg) = 0;
    virtual int addRect(double x, double y, double w, double h) = 0;
    virtual int addPoint(double x, double y) = 0;
    virtual int addText(double x, double y, const char* text, double heightMm) = 0;

    // ---- 图元查询 ----
    virtual int elementCount() const = 0;                 // 当前图元总数
    virtual bool elementExists(int id) const = 0;         // id 是否存在
    // 填充分页快照字段；返回 false 表示 id 不存在。返回的 const char* 指针
    // 在下一次本对象读调用前保持有效，插件应就地读取、勿长期持有。
    virtual bool elementInfo(int id, SdkElementInfo* out) const = 0;
    virtual const char* elementTypeName(int id) const = 0;  // UTF-8，下次读前有效
    virtual const char* elementLayer(int id) const = 0;     // UTF-8，下次读前有效
    virtual int elementPointCount(int id) const = 0;       // 控制点数量
    // 取第 index 个控制点（0-based）；越界返回 false。仅二维图元 z 恒为 0。
    virtual bool elementPoint(int id, int index, SdkPoint* out) const = 0;

    // ---- 图元删除：成功返回 true ----
    virtual bool removeElement(int id) = 0;

    // ---- 日志：写入宿主日志面板（type 由宿主按 "plugin" 归类）----
    virtual void log(const char* msg) = 0;
};

} // namespace sdk
} // namespace SketchCAD
