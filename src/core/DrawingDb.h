#pragma once
// DrawingDb — 绘图数据库存储层
//
// 设计原则：
//  - 仅负责「JSON 文本 <-> 数据库字段」的存取，完全不依赖 SketchModel / 渲染逻辑，
//    因此可被独立编译、独立测试（无头即可）。模型与 JSON 的互转由 MainWindow 复用既有
//    toDrawingJson()/readDrawing() 完成（零重复渲染代码、零回归风险）。
//  - 默认 SQLite（文件级、零配置），最契合「启动检测连接、失败静默不中断」的语义。
//  - 通过 setConnectionInfo() 可切换到 MySQL 等服务器型数据库（未来适配器）；
//    只要驱动不可用或连接失败，connect() 返回 false 且不抛异常、不弹窗。
//  - 单例：整进程一个数据库连接。

#include <QObject>
#include <QString>
#include <QList>
#include <QSqlDatabase>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFile>

namespace SketchCAD {

class DrawingDb {
public:
    // 单例入口
    static DrawingDb* instance();

    // 注册 SQL 插件搜索路径：把程序目录下的 sqldrivers/ 加入 Qt 插件搜索路径，
    // 并把可能放置 libmysql.dll 的目录注入 PATH，使「用户放入 qsqlmysql.dll 即可启用
    // MySQL」生效。若当前 Qt 构建缺失该插件，本函数不改变行为（仅保证未来可热插拔）。
    static void registerSqlPluginPaths();

    // 设置连接信息。
    //  - driver: "QSQLITE"（默认）或 "QMYSQL" 等。
    //  - 对于 SQLite：dbPath 为数据库文件路径；host/dbName/user/pass 忽略。
    //  - 对于服务器型：host 为地址，dbName 为库名，user/pass 为凭据。
    // 调用 connect() 之前设置即可生效。
    void setConnectionInfo(const QString& driver,
                           const QString& dbPath,
                           const QString& host,
                           const QString& dbName,
                           const QString& user,
                           const QString& pass);

    // 尝试连接并建表。失败（驱动缺失/路径不可写/服务器不可达）时返回 false，
    // 不抛异常、不弹窗；调用方据此静默降级到纯文件存储。
    bool connect();

    // 用临时连接测试给定连接信息是否可用（不污染单例连接）。
    // 返回空字符串表示连接成功；非空为错误描述（含驱动不可用 / 连接失败原因）。
    static QString testConnection(const QString& driver, const QString& dbPath,
                                 const QString& host, int port,
                                 const QString& dbName, const QString& user,
                                 const QString& pass);

    // 是否已连接
    bool isConnected() const { return connected_; }

    // 最近一次错误信息（连接失败时供诊断，UI 上默认不展示）
    QString lastError() const { return lastError_; }

    // ---- 配置文件（本地 JSON）----
    // 默认路径：~/SketchCAD/db_config.json（目录不存在则自动创建；始终可写）。
    // 程序启动后读取该文件作为数据库连接输入；保存配置时写回该文件。
    static QString defaultConfigPath();
    // 默认 SQLite 数据库文件路径（~/SketchCAD/sketchcad_drawings.db），供 connect() 与测试连接共用
    static QString defaultDbPath();
    // 读取配置文件并应用到连接信息；文件不存在/损坏则返回 false，调用方回落默认 SQLite 路径。
    bool loadConfigFromFile(const QString& path = defaultConfigPath());
    // 将当前连接信息写入配置文件（QSaveFile 原子写，失败返回 false 并置 lastError_）。
    bool saveConfigToFile(const QString& path = defaultConfigPath());

    // 断开当前连接（供重新配置后重连；不影响配置与文件）。
    void closeConnection();

    // 当前连接信息（供配置对话框预填）
    QString driver() const { return driver_; }
    QString dbPath() const { return dbPath_; }
    QString host()   const { return host_; }
    QString dbName() const { return dbName_; }
    QString user()   const { return user_; }
    QString pass()   const { return pass_; }
    int     port()   const { return port_; }
    void setPort(int p) { port_ = p; }

    // ---------- 图档（drawings）----------
    struct DrawingRecord {
        int     id = 0;
        QString name;        // 用户命名
        QString createdAt;
        QString updatedAt;
    };

    // 新增图档。json 为完整绘图 JSON 文本（来自 SketchModel::toDrawingBytes() 的 UTF-8 字符串）。
    // 成功返回 true，outId 回填自增主键。
    bool insertDrawing(const QString& name, const QString& json, int* outId = nullptr);

    // 更新已有图档（内容 + 名称 + updated_at）
    bool updateDrawing(int id, const QString& name, const QString& json);

    // 删除图档
    bool deleteDrawing(int id);

    // 列出图档元数据（不含 json，避免一次性加载全部大文本）。按更新时间倒序。
    QList<DrawingRecord> listDrawings();

    // 按 id 取回完整 JSON 文本（供打开/渲染）
    QString getDrawingJson(int id);

    // ---------- 模板（templates）----------
    // 模板 = 一段「相同形状」的几何，外加创建时记录的图纸参考尺寸（refWidth/refHeight），
    // 以便后续按「不同尺寸」套用：factor = 目标尺寸 / 参考尺寸，再用
    // SketchModel::setScaleRatio() 围绕内容中心等比缩放。
    struct TemplateRecord {
        int     id = 0;
        QString name;        // 模板命名
        double  refWidth  = 0.0;   // 保存时的图纸宽度(mm)
        double  refHeight = 0.0;   // 保存时的图纸高度(mm)
        QString createdAt;
    };

    // 新增模板。json 为模板几何的完整 JSON 文本。
    bool insertTemplate(const QString& name,
                        const QString& json,
                        double refWidth,
                        double refHeight,
                        int* outId = nullptr);

    // 删除模板
    bool deleteTemplate(int id);

    // 列出模板元数据（不含 json，含参考尺寸）。按创建时间倒序。
    QList<TemplateRecord> listTemplates();

    // 按 id 取回模板完整 JSON 文本
    QString getTemplateJson(int id);

    // 取回模板参考尺寸
    bool getTemplateRefSize(int id, double* refWidth, double* refHeight);

private:
    DrawingDb();
    ~DrawingDb();
    DrawingDb(const DrawingDb&) = delete;
    DrawingDb& operator=(const DrawingDb&) = delete;

    bool ensureSchema();
    // 修复旧版误建 schema：若 table.name 为时间类型则改为文本类型（仅 MySQL 用）。
    void migrateNameColumnToText(QSqlQuery& q, const QString& table, const QString& textType);
    QString nowStamp() const;

    static const char* kConnectionName;

    QString driver_   = QStringLiteral("QSQLITE");
    QString dbPath_;            // SQLite 文件路径（默认见 connect() 内推导）
    QString host_;
    QString dbName_;
    QString user_;
    QString pass_;
    int     port_   = 3306;     // 服务器型数据库端口（MySQL 默认 3306）

    bool    connected_ = false;
    QString lastError_;

    QSqlDatabase db_;
};

} // namespace SketchCAD
