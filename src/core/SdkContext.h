#pragma once

#include "SketchCAD/i_application.h"
#include "SketchCAD/i_model.h"
#include "SketchCAD/types.h"
#include <QString>

// ============================================================================
// 宿主侧 SDK 桥接（内部实现，编译进主程序 / 自测）
// ----------------------------------------------------------------------------
// SdkApplication / SdkModel 把宿主核心能力（SketchModel + ElementFactory +
// CommandInterpreter）投影为 SDK 的稳定公共接口（IApplication / IModel）。
// 它们是「纯 C++ 抽象接口的具体实现」，仅存在于宿主二进制内；
// 插件通过 initialize(IApplication*) 拿到的就是 SdkApplication*，经虚函数
// 调用反向访问宿主，无需链接宿主任何内部符号。
//
// 本文件不含任何 Qt / STL 跨边界类型泄漏——对外一律 POD / const char*。
// ============================================================================

namespace SketchCAD {

class SketchModel;
class CommandInterpreter;

// 数据库桥接：实现 sdk::IModel。
class SdkModel : public sdk::IModel {
public:
    explicit SdkModel(SketchModel* m) : m_(m) {}

    int  addLine(double x1, double y1, double x2, double y2) override;
    int  addCircle(double cx, double cy, double r) override;
    int  addArc(double cx, double cy, double r, double startDeg, double endDeg) override;
    int  addRect(double x, double y, double w, double h) override;
    int  addPoint(double x, double y) override;
    int  addText(double x, double y, const char* text, double heightMm) override;

    int  elementCount() const override;
    bool elementExists(int id) const override;
    bool elementInfo(int id, sdk::SdkElementInfo* out) const override;
    const char* elementTypeName(int id) const override;
    const char* elementLayer(int id) const override;
    int  elementPointCount(int id) const override;
    bool elementPoint(int id, int index, sdk::SdkPoint* out) const override;

    bool removeElement(int id) override;
    void log(const char* msg) override;

    // 多文档切换重绑：插件桥接的模型随活动文档换
    void setModel(SketchModel* m) { m_ = m; }

    SketchModel* raw() const { return m_; }

private:
    SketchModel*       m_ = nullptr;
    mutable QByteArray layerBuf_;   // elementLayer 返回的 UTF-8 缓冲（下次读前有效）
};

// 运行时桥接：实现 sdk::IApplication。
class SdkApplication : public sdk::IApplication {
public:
    SdkApplication(SketchModel* m, CommandInterpreter* interp);

    sdk::IModel* model() override { return &modelBridge_; }
    void         log(const char* msg) override;
    bool         registerCommand(const char* name, sdk::SdkCommandFn fn) override;

    const char* appVersion() const override;
    int         sdkVersion() const override;
    int         sdkAbiVersion() const override;

    SketchModel*       rawModel() const { return m_; }
    CommandInterpreter* rawInterp() const { return interp_; }

    // 多文档切换重绑：应用桥与模型桥一起换绑
    void setModel(SketchModel* m) { m_ = m; modelBridge_.setModel(m); }

private:
    SketchModel*      m_ = nullptr;
    CommandInterpreter* interp_ = nullptr;
    SdkModel          modelBridge_;
};

} // namespace SketchCAD
