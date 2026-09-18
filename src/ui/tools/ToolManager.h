#pragma once
#include <QString>
#include <QHash>
#include <map>
#include <memory>
#include "ui/tools/ITool.h"

namespace SketchCAD {

// ============================================================
// ToolManager —— 工具管理器（Strategy + Factory）
// ============================================================
// 职责：
//   1. 注册所有工具实例（Factory：工具 ID → ITool 映射）
//   2. 管理活动工具的生命周期（activate / deactivate）
//   3. 将鼠标事件与提示请求转发给活动工具（Strategy 分派）
//
// 设计模式：
//   - Strategy：ITool 是策略接口，ToolManager 是 Context 类
//   - Factory：registerTool() 构建工具 ID → 实例的映射表
//
// DRY 收益：
//   - GraphicsView::handlePress() 的 80 行 if-else → toolMgr_->dispatchPress()
//   - GraphicsView::updateCommandPrompt() 的 140 行 if-else → toolMgr_->dispatchPrompt()
//   - 新增工具只需创建 ITool 子类 + 在 registerBuiltins() 中添加一行
class ToolManager {
public:
    explicit ToolManager(ToolContext ctx);

    /// 注册工具实例
    void registerTool(const QString& id, std::unique_ptr<ITool> tool);

    /// 设置活动工具（触发旧工具 onDeactivate + 新工具 onActivate）
    void setActiveTool(const QString& id);

    /// 获取当前活动工具 ID
    QString activeToolId() const { return activeId_; }

    // ---- 事件分派（Strategy Pattern 核心）----
    QString dispatchPrompt() const;                                           ///< 生成命令行提示
    bool    dispatchPress(const Vec3& w, QMouseEvent* e);                    ///< 左键按下
    bool    dispatchMove(const Vec3& w, QMouseEvent* e);                     ///< 鼠标移动
    bool    dispatchRelease(const Vec3& w, QMouseEvent* e);                  ///< 左键释放
    bool    dispatchRightButton();                                            ///< 右键按下

    /// 当前是否注册了指定工具
    bool hasTool(const QString& id) const { return tools_.count(id) > 0; }

private:
    void registerBuiltins();   ///< 注册所有内置工具

    ToolContext ctx_;
    // 使用 std::map 而非 QHash：unique_ptr 不可拷贝，QHash 的 operator[] / value()
    // 需要拷贝语义，std::map 支持 move-only 类型
    std::map<QString, std::unique_ptr<ITool>> tools_;
    QString activeId_;
    ITool* active_ = nullptr;   ///< 缓存当前活动工具指针（避免每次查表）
};

} // namespace SketchCAD
