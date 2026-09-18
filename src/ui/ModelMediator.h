#pragma once
#include <QObject>
#include <QColor>
#include <QString>
#include <functional>

class QWidget;

namespace SketchCAD {

class SketchModel;
class GraphicsView;
class LogPanel;
class ElementPanel;
class BomTree;
class PropertyPanel;
class StatusBar;
class RulerWidget;
class CommandLine;

// 中介者（Mediator Pattern）：把「模型 ↔ 视图 ↔ 面板 ↔ 状态栏 ↔ 标尺 ↔ 命令行」之间
// 的所有信号接线集中到这一处。主旨是「单一触发时机」：
//   1) 谁订阅谁、触发几次，只在这一处定义，杜绝"面板自连接 vs 主窗口连接"的双份/遗漏。
//   2) 新增一个模型的观察者，只需在 wire() 里加一行，不用分散到各面板构造函数。
// 凡反应需要触碰主窗口私有成员（工具条高亮 / 参数条 / 样式回显 / 标题栏比例 / 连续直线开关），
// 通过 Host 回调委托给主窗口；其余纯组件联动（状态栏、标尺、日志、图元面板↔模型）在此直接接线。
class ModelMediator : public QObject {
    Q_OBJECT
public:
    // 需要触摸主窗口私有成员的反应，由主窗口注入 lambda。只放"非纯组件联动"的少数几类。
    struct Host {
        std::function<void(const QString&)> onActiveToolChanged;   // 工具切换 → 工具条高亮 + 参数条
        std::function<void(const QColor&, double)> onDefaultStyleChanged;      // 默认颜色+线宽回显
        std::function<void(int)> onDefaultLineStyleChanged;                   // 默认线型回显（含线宽档位）
        std::function<void()> onScaleChanged;                     // 绘图比例变化 → 标题栏「比例」栏
        std::function<void(bool)> onChainToggled;                 // 连续直线开关回显到工具条按钮
    };

    struct Components {
        SketchModel* model = nullptr;
        GraphicsView* view = nullptr;
        LogPanel* logPanel = nullptr;
        ElementPanel* elemPanel = nullptr;
        BomTree* bom = nullptr;
        PropertyPanel* propPanel = nullptr;
        StatusBar* statusBar = nullptr;
        RulerWidget* hRuler = nullptr;
        RulerWidget* vRuler = nullptr;
        CommandLine* cmdLine = nullptr;
    };

    ModelMediator(QObject* parent, const Components& comp);
    void setHost(const Host& host);
    void wire();   // 集中接线：可显式调用；构造函数并不会自动 wire，便于测试按需控制

private:
    Components c_;
    Host host_;
};

} // namespace SketchCAD