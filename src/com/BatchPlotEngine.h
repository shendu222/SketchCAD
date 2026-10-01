#pragma once
// ============================================================================
// 批量出图引擎（无头 / headless，与 COM 分派层完全解耦）
// ----------------------------------------------------------------------------
// 设计要点：
//   1) 引擎层不包含任何 COM 类型，纯 Qt + 核心层实现，便于 SketchSelfTest
//      无 COM 依赖直接测试（t_comAutomation）；
//   2) 读档：复用核心层既有通路 —— JSON 走 SketchCAD::elementFromJson +
//      SketchModel 各 set*FromJson（与 MainWindow::readDrawing 同语义，缺键
//      容缺向后兼容）；DXF/DWG 走 cadfmt::fromDxf/fromDwg + $INSUNITS 换算；
//   3) 出图：PDF 走 PrintEngine::exportPdf（QPdfWriter，offscreen 可行），
//      DXF/DWG/STEP/JSON 走 cadfmt::to*（BlockRef 先展开，导出物即所见）；
//   4) 并发稳定：进程级互斥锁保证同一进程内批量任务串行执行；Qt 应用对象
//      懒初始化（宿主进程未构造 QApplication 时补建，构造同样受锁保护）；
//   5) 事件：引擎层通过 std::function 事件槽上报（onTaskStart/onTaskDone/
//      onBatchFinished），COM 层把它桥接到调用方注册的 IDispatch 事件宿主；
//      自测层则用 lambda 直接接收；
//   6) 日志：可选日志文件，逐任务追加一行（时间戳 + 任务号 + 状态 + 耗时），
//      批量结束追加汇总行。
// ============================================================================
#include <QString>
#include <QJsonObject>
#include <QVector>
#include <functional>

namespace SketchCAD {

class SketchModel;   // ⚠️ 必须在 namespace 内前向声明，否则声明的是全局 ::SketchModel

namespace comauto {

// ---- 批量任务定义 ----
struct BatchTask {
    QString     source;    // 输入图纸路径（.json/.dxf/.dwg）
    QString     output;    // 输出路径（.pdf/.dxf/.dwg/.step/.json，按扩展名分派）
    QJsonObject settings;  // 可选：PrintSettings JSON（仅 PDF 输出消费；空对象 = 默认版式）
};

struct BatchTaskResult {
    int     index  = -1;     // 任务序号（0 起）
    QString source;
    QString output;
    bool    ok     = false;
    QString error;           // 失败原因（成功为空）
    qint64  bytes   = 0;     // 输出文件字节数
    qint64  elapsedMs = 0;   // 单任务耗时
};

struct BatchResult {
    bool allOk = false;
    int  total = 0, succeeded = 0, failed = 0;
    QVector<BatchTaskResult> tasks;

    // 汇总结果 JSON（GetTaskStatus 返回值；供外部系统程序化读取）
    // { "allOk":true, "total":3, "succeeded":2, "failed":1,
    //   "tasks":[{"index":0,"source":..,"output":..,"ok":true,"bytes":1234,"elapsedMs":56,"error":""}, ...] }
    QJsonObject toJson() const;
};

class BatchPlotEngine {
public:
    // 事件回调：event ∈ {"onTaskStart","onTaskDone","onBatchFinished"}，
    // payload 为对应 JSON 对象（字段见 runTasks 实现内注释）。
    using EventFn = std::function<void(const QString& event, const QJsonObject& payload)>;

    // 注册进程级唯一事件槽（COM 层在 RegisterCallback 时调用；传空 = 注销）。
    static void setEventSink(EventFn fn);

    // 确保当前进程存在 QCoreApplication/QApplication。
    // 宿主已有实例（GUI 程序或自测）直接复用；否则在互斥保护下懒构造。
    static void ensureQtApplication();

    // ---- 单步能力（也可单独使用）----
    // 无头读档：JSON 完整往返 / DXF/DWG 导入（含单位换算与弧段降级统计）。
    // 成功返回 true；失败把原因写入 *err。
    static bool loadDrawing(SketchModel& model, const QString& path, QString* err);

    // 无头导出：按输出扩展名分派 pdf/dxf/dwg/step/json。
    // settings 仅 PDF 消费（PrintSettings::fromJson，空对象用默认版式）。
    static bool exportDrawing(SketchModel& model, const QString& path,
                              const QJsonObject& settings, QString* err);

    // ---- 批量执行 ----
    // tasksJson 既接受对象 {"tasks":[...]} 也接受裸数组 [...]，
    // 每项 {"source":"..","output":"..","settings":{..}}（settings 可省）。
    // 解析失败返回 total=0 且结果首个任务带 SCAD_E_BAD_TASK 语义错误串。
    // logPath 非空时逐任务追加日志行 + 结束汇总行。
    static BatchResult runBatch(const QString& tasksJson, const QString& logPath);

    // 已解析任务列表直接执行（COM 层与自测共用的真正入口）。
    // 事件序列：每任务前 onTaskStart{index,source,output}；
    //           每任务后 onTaskDone{index,source,output,ok,error,bytes,elapsedMs}；
    //           全部后 onBatchFinished{allOk,total,succeeded,failed}。
    static BatchResult runTasks(const QVector<BatchTask>& tasks, const QString& logPath);

private:
    static void appendLog(const QString& logPath, const QString& line);
};

} // namespace comauto
} // namespace SketchCAD
