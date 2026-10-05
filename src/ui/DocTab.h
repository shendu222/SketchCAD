#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

namespace SketchCAD {
class SketchModel;

// ============================================================================
// 多文档标签（DocTab）：文档会话 + 会话管理器
// ----------------------------------------------------------------------------
// 设计要点（与任务边界一一对应）：
//   * 每个 DocSession 拥有一份独立的 SketchModel（parent 到会话上，随会话销毁）。
//     SketchModel 自带 undo/redo 双栈、选择集、UCS、活动工具 ⇒ 「切换文档时
//     撤销栈/选择集/坐标系/活动工具随文档切换」由数据归属天然保证，
//     不存在第二份模型真相源（禁止全局单例模型）。
//   * 命令行历史 / lastCommand 属会话态：MainWindow 在切换时把它与共享的
//     CommandInterpreter 互换（copy-in / copy-out）。
//   * 脏标记：会话观察自己模型的 modelChanged 置位；读档/保存后由外部
//     markClean 复位（读档过程中的模型改动是瞬态的）。
//   * DocManager 是纯逻辑层（无 UI 依赖）：可被 SketchSelfTest 离线直测；
//     QTabBar/QStackedWidget 等 UI 由 MainWindow 驱动并与之保持同步。
// ============================================================================
class DocSession : public QObject {
    Q_OBJECT
public:
    explicit DocSession(QObject* parent = nullptr);

    SketchModel* model() const { return model_; }

    QString     filePath;        // 当前图纸路径（未存盘为空）
    QStringList cmdHistory;      // 命令行历史（切换文档时与解释器互换）
    QString     lastCommand;     // 上次成功执行的命令名（随历史一起换）
    int         untitledIndex = 0;   // 未命名序号（"绘图 N"）

    // 标签显示名：文件名（未存盘为 绘图N）；脏时追加 " *"
    QString title() const;
    bool    isDirty() const { return dirty_; }

public slots:
    void markClean() { setDirty(false); }
    void markDirty() { setDirty(true); }

signals:
    void dirtyChanged(DocSession* s);

private:
    void setDirty(bool d);       // 幂等：状态不变不发信号
    SketchModel* model_ = nullptr;
    bool dirty_ = false;
};

// ============================================================================
// 文档管理器：会话表 + 活动下标 + Ctrl+Tab 循环 + 拖拽重排 + 关闭夹取。
// 所有下标语义与 QTabBar 一致（0..count-1），MainWindow 按下标同步标签条。
// ============================================================================
class DocManager : public QObject {
    Q_OBJECT
public:
    explicit DocManager(QObject* parent = nullptr);
    ~DocManager() override;

    int         addDoc(DocSession* s);        // 追加已有会话，返回下标（首个自动成为活动）
    DocSession* addNewDoc();                  // 建空白会话（绘图 N，N 自增）
    int         count() const { return docs_.size(); }
    DocSession* doc(int i) const { return docs_.value(i, nullptr); }
    DocSession* active() const { return doc(active_); }
    int         activeIndex() const { return active_; }
    void        setActive(int i);             // 越界忽略；变化时发射 activeChanged

    int  cycle(int step = 1) const;           // Ctrl+Tab：返回下一活动下标（不改状态）
    void reorder(int from, int to);           // 标签拖拽重排（活动身份跟随会话）
    void removeDoc(int i);                    // 关闭：销毁会话（含模型）；活动夹取到邻居

    bool isDirty(int i) const;
    void markClean(int i);
    void markDirty(int i);

signals:
    void docAdded(int i);
    void docRemoved(int i);
    void activeChanged(int i);
    void dirtyChanged(int i);                 // i 的脏标记变化（刷新标签文本）
    void orderChanged();                      // 重排完成

private:
    QList<DocSession*> docs_;
    int active_ = -1;
    int untitledSeq_ = 0;
};

} // namespace SketchCAD
