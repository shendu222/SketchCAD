#include "DrawingDb.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QDateTime>
#include <QVariant>
#include <QJsonDocument>
#include <QSaveFile>
#include <QFile>
#include <QCoreApplication>
#include <QLibrary>
#include <QPluginLoader>
#include "JsonUtil.h"

namespace SketchCAD {

const char* DrawingDb::kConnectionName = "sketchcad_db";

DrawingDb::DrawingDb() {}
DrawingDb::~DrawingDb() {
    if (connected_ && db_.isOpen()) db_.close();
    if (QSqlDatabase::contains(kConnectionName))
        QSqlDatabase::removeDatabase(kConnectionName);
}

DrawingDb* DrawingDb::instance() {
    static DrawingDb s_instance;
    // 首次访问即确保 SQL 插件路径已注册，使后续 isDriverAvailable / addDatabase
    // 能发现用户放入的 qsqlmysql.dll（无需重启即可在下次打开配置时生效）。
    registerSqlPluginPaths();
    return &s_instance;
}

void DrawingDb::registerSqlPluginPaths() {
    static bool done = false;
    if (done) return;
    done = true;

    // 让 <程序目录> 与 <程序目录>/sqldrivers 都进入 Qt 插件搜索路径：
    // 用户把 qsqlmysql.dll 丢进 sqldrivers/ 即可启用 MySQL（Qt 默认会在 libraryPath 下找 sqldrivers 子目录）。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString plugDir = appDir + QStringLiteral("/sqldrivers");
    QCoreApplication::addLibraryPath(plugDir);
    QCoreApplication::addLibraryPath(appDir);

    // 若相邻/常见位置存在 MySQL 客户端库 libmysql.dll，提前注入 PATH，
    // 便于 qsqlmysql.dll 在运行时动态加载到客户端库（否则插件加载会因缺 libmysql 而失败）。
    const QStringList candidates = {
        plugDir + QStringLiteral("/libmysql.dll"),
        appDir + QStringLiteral("/libmysql.dll"),
        QStringLiteral("C:/Program Files/MySQL/MySQL Server 9.6/bin/libmysql.dll"),
        QStringLiteral("C:/Program Files/MySQL/MySQL Server 9.6/lib/libmysql.dll"),
        QStringLiteral("C:/Program Files/MySQL/MySQL Server 8.0/bin/libmysql.dll"),
        QStringLiteral("C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll")
    };
    for (const QString& c : candidates) {
        if (QFile::exists(c)) {
            const QString dir = QFileInfo(c).absolutePath();
            const QString cur = QString::fromLocal8Bit(qgetenv("PATH").constData());
            if (!cur.split(QLatin1Char(';'), Qt::SkipEmptyParts).contains(dir)) {
                qputenv("PATH", (dir + QLatin1Char(';') + cur).toLocal8Bit());
            }
            // 关键修复：显式预加载 libmysql.dll（按完整路径 LoadLibrary）。
            // Windows 对 DLL 的「隐式依赖」默认只搜索 exe 目录与 PATH，并不搜索依赖 DLL
            // 自身所在目录；仅靠 PATH 注入对加载器不够稳。这里用 QLibrary 按完整路径
            // 先把正确的 libmysql 载入进程，之后 qsqlmysql.dll 的隐式导入会复用已加载的模块，
            // 避免被 PATH 上其它版本（如 8.0）顶替导致 QMYSQLDriver 构造失败（Driver not loaded）。
            QLibrary lib(c);
            lib.setLoadHints(QLibrary::ResolveAllSymbolsHint);
            if (!lib.load())
                qWarning() << "[DrawingDb] 预加载 libmysql.dll 失败:" << c << lib.errorString();
            break;
        }
    }
}

void DrawingDb::setConnectionInfo(const QString& driver,
                                  const QString& dbPath,
                                  const QString& host,
                                  const QString& dbName,
                                  const QString& user,
                                  const QString& pass) {
    driver_ = driver.isEmpty() ? QStringLiteral("QSQLITE") : driver;
    dbPath_ = dbPath;
    host_   = host;
    dbName_ = dbName;
    user_   = user;
    pass_   = pass;
}

QString DrawingDb::nowStamp() const {
    // 同时兼容 SQLite(TEXT) 与 MySQL(DATETIME) 的字符串格式
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

bool DrawingDb::connect() {
    if (connected_) return true;

    // 驱动不可用 → 静默降级
    if (!QSqlDatabase::isDriverAvailable(driver_)) {
        connected_ = false;
        lastError_ = QStringLiteral("数据库驱动不可用: ") + driver_;
        return false;
    }

    // 清理可能残留的同名连接，保证重试安全
    {
        QSqlDatabase stale = QSqlDatabase::database(kConnectionName);
        if (stale.isValid() && stale.isOpen()) stale.close();
    }
    if (QSqlDatabase::contains(kConnectionName))
        QSqlDatabase::removeDatabase(kConnectionName);

    db_ = QSqlDatabase::addDatabase(driver_, kConnectionName);
    if (driver_ == QStringLiteral("QSQLITE")) {
        QString path = dbPath_;
        if (path.isEmpty()) path = defaultDbPath();
        db_.setDatabaseName(path);
    } else {
        db_.setHostName(host_);
        db_.setDatabaseName(dbName_);
        db_.setUserName(user_);
        db_.setPassword(pass_);
        db_.setPort(port_);
    }

    if (!db_.open()) {
        connected_ = false;
        lastError_ = db_.lastError().text();
        return false;
    }

    if (!ensureSchema()) {
        connected_ = false;
        return false;
    }

    connected_ = true;
    return true;
}

bool DrawingDb::ensureSchema() {
    // 依驱动生成兼容的建表语句（当前默认 SQLite；MySQL 适配器已预留）
    // ⚠️ name 列必须是文本类型（图档/模板名，可含中文与图层名如 C0Layer），
    //    绝不能是时间类型。旧版曾误把 tsType(DATETIME) 填到 %2，导致 MySQL 下
    //    name 被建成 DATETIME，写入字符串时报 "Incorrect datetime value"。
    QString idCol, nameType, tsType, jsonType;
    if (driver_ == QStringLiteral("QSQLITE")) {
        idCol    = QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT");
        nameType = QStringLiteral("TEXT");
        tsType   = QStringLiteral("TEXT");
        jsonType = QStringLiteral("TEXT");
    } else {
        // 通用服务器型（MySQL 等）
        idCol    = QStringLiteral("INT PRIMARY KEY AUTO_INCREMENT");
        nameType = QStringLiteral("VARCHAR(255)");
        tsType   = QStringLiteral("DATETIME");
        jsonType = QStringLiteral("LONGTEXT");
    }

    const QString drawsSql =
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS drawings ("
            "  id %1,"
            "  name %2 NOT NULL,"
            "  json %3 NOT NULL,"
            "  created_at %4 NOT NULL,"
            "  updated_at %4 NOT NULL"
            ")").arg(idCol, nameType, jsonType, tsType);

    const QString templSql =
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS templates ("
            "  id %1,"
            "  name %2 NOT NULL,"
            "  json %3 NOT NULL,"
            "  ref_width DOUBLE NOT NULL DEFAULT 0,"
            "  ref_height DOUBLE NOT NULL DEFAULT 0,"
            "  created_at %4 NOT NULL"
            ")").arg(idCol, nameType, jsonType, tsType);

    QSqlQuery q(db_);
    if (!q.exec(drawsSql)) {
        lastError_ = q.lastError().text();
        db_.close();
        return false;
    }
    if (!q.exec(templSql)) {
        lastError_ = q.lastError().text();
        db_.close();
        return false;
    }

    // 迁移：既有表若因旧版建表语句把 name 误建成时间类型（MySQL 严格类型），
    // 改为文本类型。仅 MySQL 需要（SQLite 文本亲和，已能存任意字符串）。
    // CREATE TABLE IF NOT EXISTS 不会改写已存在的表，故必须显式 ALTER 修复。
    if (driver_ != QStringLiteral("QSQLITE")) {
        migrateNameColumnToText(q, QStringLiteral("drawings"),  nameType);
        migrateNameColumnToText(q, QStringLiteral("templates"), nameType);
    }
    return true;
}

// 若 table.name 当前为时间类型（datetime/timestamp/date/time），改写成文本类型。
// 仅用于修复旧版误建 schema；正常新建表不会命中。
void DrawingDb::migrateNameColumnToText(QSqlQuery& q, const QString& table,
                                        const QString& textType) {
    q.clear();
    if (!q.exec(QStringLiteral("SHOW COLUMNS FROM %1 LIKE 'name'").arg(table)))
        return;
    if (!q.next()) return;
    const QString type = q.value(1).toString().toLower();
    if (type.contains(QStringLiteral("datetime")) || type.contains(QStringLiteral("timestamp")) ||
        type.contains(QStringLiteral("date"))     || type.contains(QStringLiteral("time"))) {
        q.clear();
        q.exec(QStringLiteral("ALTER TABLE %1 MODIFY name %2 NOT NULL").arg(table, textType));
    }
}

// ---------------- 配置文件（本地 JSON） ----------------

QString DrawingDb::defaultConfigPath() {
    QDir home(QDir::home());
    if (!home.exists(QStringLiteral("SketchCAD")))
        home.mkpath(QStringLiteral("SketchCAD"));
    return home.filePath(QStringLiteral("SketchCAD/db_config.json"));
}

QString DrawingDb::defaultDbPath() {
    QDir home(QDir::home());
    if (!home.exists(QStringLiteral("SketchCAD")))
        home.mkpath(QStringLiteral("SketchCAD"));
    return home.filePath(QStringLiteral("SketchCAD/sketchcad_drawings.db"));
}

bool DrawingDb::loadConfigFromFile(const QString& path) {
    QFile f(path);
    if (!f.exists()) return false;          // 无配置文件：调用方回落默认 SQLite 路径
    if (!f.open(QIODevice::ReadOnly)) {
        lastError_ = f.errorString();
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError perr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &perr);
    if (doc.isNull() || !doc.isObject()) {
        lastError_ = perr.errorString();
        return false;
    }
    const QJsonObject o = doc.object();

    QString driver = json::getString(o, "driver", driver_);
    // 仅接受已知驱动，防止误配置导致后续 connect 静默失败
    if (driver != QStringLiteral("QSQLITE") && driver != QStringLiteral("QMYSQL"))
        driver = QStringLiteral("QSQLITE");
    const QString dbPath = json::getString(o, "dbPath", dbPath_);
    const QString host   = json::getString(o, "host", host_);
    const QString dbName = json::getString(o, "dbName", dbName_);
    const QString user   = json::getString(o, "user", user_);
    const QString pass   = json::getString(o, "pass", pass_);
    const int     port   = json::getInt(o, "port", port_);

    setConnectionInfo(driver, dbPath, host, dbName, user, pass);
    setPort(port);
    return true;
}

bool DrawingDb::saveConfigToFile(const QString& path) {
    QJsonObject o;
    o["version"] = 1;
    o["driver"]  = driver_;
    o["dbPath"]  = dbPath_;
    o["host"]    = host_;
    o["dbName"]  = dbName_;
    o["user"]    = user_;
    o["pass"]    = pass_;
    o["port"]    = port_;

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        lastError_ = f.errorString();
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    if (!f.commit()) {                  // 原子写：成功才替换原文件，避免半截配置
        lastError_ = f.errorString();
        return false;
    }
    return true;
}

void DrawingDb::closeConnection() {
    if (connected_ && db_.isOpen()) db_.close();
    connected_ = false;
    if (QSqlDatabase::contains(kConnectionName))
        QSqlDatabase::removeDatabase(kConnectionName);
}

QString DrawingDb::testConnection(const QString& driver, const QString& dbPath,
                                 const QString& host, int port,
                                 const QString& dbName, const QString& user,
                                 const QString& pass) {
    // 防御：确保插件搜索路径与 libmysql 的 PATH 已注册，不依赖 instance() 是否先被调用。
    registerSqlPluginPaths();

    // 自诊断：把「运行态实际看到的路径与文件」回显到错误信息，便于定位
    // （最常见的是用户启动的不是 dist/SketchCAD.exe，其目录下没有 sqldrivers/）。
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString plugDir = appDir + QStringLiteral("/sqldrivers");
    const bool pluginThere = QFile::exists(plugDir + QStringLiteral("/qsqlmysql.dll"));
    const bool libThere    = QFile::exists(plugDir + QStringLiteral("/libmysql.dll"));
    const QString diag = QStringLiteral(
        "\n[诊断] appDir=%1\n       sqldrivers/qsqlmysql.dll=%2   sqldrivers/libmysql.dll=%3")
        .arg(appDir,
             pluginThere ? QStringLiteral("存在") : QStringLiteral("缺失"),
             libThere    ? QStringLiteral("存在") : QStringLiteral("缺失"));

    if (!QSqlDatabase::isDriverAvailable(driver))
        return QStringLiteral(
            "数据库驱动不可用：%1。\n当前 Qt 构建未包含该数据库驱动插件（需要 qsqlmysql.dll 与 libmysql.dll）。\n"
            "请将这两个文件放入程序目录的 sqldrivers/ 子目录后重启，或改用内置的 SQLite（文件型，零配置）。")
            .arg(driver) + diag;

    const QString connName = QStringLiteral("sketchcad_db_test");
    if (QSqlDatabase::contains(connName)) QSqlDatabase::removeDatabase(connName);

    QString err;
    {
        QSqlDatabase t = QSqlDatabase::addDatabase(driver, connName);
        if (driver == QStringLiteral("QSQLITE")) {
            QString p = dbPath;
            if (p.isEmpty()) p = defaultDbPath();
            t.setDatabaseName(p);
        } else {
            t.setHostName(host);
            t.setDatabaseName(dbName);
            t.setUserName(user);
            t.setPassword(pass);
            t.setPort(port);
        }
        if (t.open()) {
            t.close();
        } else {
            err = t.lastError().text();
            // 深层诊断：直接按完整路径加载插件并试图实例化驱动，捕获底层失败原因；
            // 同时探测 libmysql 实际命中哪个文件，定位「隐式依赖被 PATH 上其它版本顶替」之类问题。
            QPluginLoader pl(plugDir + QStringLiteral("/qsqlmysql.dll"));
            const bool loaded = pl.load();
            QObject* inst = pl.instance();
            err += QStringLiteral("\n[深层] 插件load=%1 instance=%2 loadErr=%3")
                .arg(loaded ? QStringLiteral("OK") : QStringLiteral("FAIL"),
                     inst ? QStringLiteral("OK") : QStringLiteral("NULL"),
                     pl.errorString());
            QLibrary lm(QStringLiteral("libmysql"));
            err += QStringLiteral("\n[深层] libmysql 命中=%1")
                .arg(lm.load() ? lm.fileName() : lm.errorString());
        }
    }
    QSqlDatabase::removeDatabase(connName);
    if (!err.isEmpty()) err += diag;
    return err;
}

// ---------------- 图档 ----------------

bool DrawingDb::insertDrawing(const QString& name, const QString& json, int* outId) {
    if (!connected_) return false;
    const QString ts = nowStamp();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "INSERT INTO drawings (name, json, created_at, updated_at) "
        "VALUES (?, ?, ?, ?)"));
    q.addBindValue(name);
    q.addBindValue(json);
    q.addBindValue(ts);
    q.addBindValue(ts);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    if (outId) *outId = q.lastInsertId().toInt();
    return true;
}

bool DrawingDb::updateDrawing(int id, const QString& name, const QString& json) {
    if (!connected_) return false;
    const QString ts = nowStamp();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "UPDATE drawings SET name = ?, json = ?, updated_at = ? WHERE id = ?"));
    q.addBindValue(name);
    q.addBindValue(json);
    q.addBindValue(ts);
    q.addBindValue(id);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    return q.numRowsAffected() >= 0;
}

bool DrawingDb::deleteDrawing(int id) {
    if (!connected_) return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM drawings WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    return true;
}

QList<DrawingDb::DrawingRecord> DrawingDb::listDrawings() {
    QList<DrawingRecord> out;
    if (!connected_) return out;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "SELECT id, name, created_at, updated_at FROM drawings "
        "ORDER BY updated_at DESC"));
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return out;
    }
    while (q.next()) {
        DrawingRecord r;
        r.id        = q.value(0).toInt();
        r.name      = q.value(1).toString();
        r.createdAt = q.value(2).toString();
        r.updatedAt = q.value(3).toString();
        out.append(r);
    }
    return out;
}

QString DrawingDb::getDrawingJson(int id) {
    if (!connected_) return QString();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT json FROM drawings WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        lastError_ = q.lastError().text();
        return QString();
    }
    return q.value(0).toString();
}

// ---------------- 模板 ----------------

bool DrawingDb::insertTemplate(const QString& name,
                               const QString& json,
                               double refWidth,
                               double refHeight,
                               int* outId) {
    if (!connected_) return false;
    const QString ts = nowStamp();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "INSERT INTO templates (name, json, ref_width, ref_height, created_at) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(name);
    q.addBindValue(json);
    q.addBindValue(refWidth);
    q.addBindValue(refHeight);
    q.addBindValue(ts);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    if (outId) *outId = q.lastInsertId().toInt();
    return true;
}

bool DrawingDb::deleteTemplate(int id) {
    if (!connected_) return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("DELETE FROM templates WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return false;
    }
    return true;
}

QList<DrawingDb::TemplateRecord> DrawingDb::listTemplates() {
    QList<TemplateRecord> out;
    if (!connected_) return out;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral(
        "SELECT id, name, ref_width, ref_height, created_at FROM templates "
        "ORDER BY created_at DESC"));
    if (!q.exec()) {
        lastError_ = q.lastError().text();
        return out;
    }
    while (q.next()) {
        TemplateRecord r;
        r.id        = q.value(0).toInt();
        r.name      = q.value(1).toString();
        r.refWidth  = q.value(2).toDouble();
        r.refHeight = q.value(3).toDouble();
        r.createdAt = q.value(4).toString();
        out.append(r);
    }
    return out;
}

QString DrawingDb::getTemplateJson(int id) {
    if (!connected_) return QString();
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT json FROM templates WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        lastError_ = q.lastError().text();
        return QString();
    }
    return q.value(0).toString();
}

bool DrawingDb::getTemplateRefSize(int id, double* refWidth, double* refHeight) {
    if (!connected_) return false;
    QSqlQuery q(db_);
    q.prepare(QStringLiteral("SELECT ref_width, ref_height FROM templates WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec() || !q.next()) {
        lastError_ = q.lastError().text();
        return false;
    }
    if (refWidth)  *refWidth  = q.value(0).toDouble();
    if (refHeight) *refHeight = q.value(1).toDouble();
    return true;
}

} // namespace SketchCAD
