# 第 8 章 C++ 封装与综合实战

## 本章目标

- 用 RAII 思想封装 `sqlite3*` 与 `sqlite3_stmt*`，让资源管理自动化；
- 实现一个线程安全友好的 `Database` / `Statement` 小型封装（不到 200 行）；
- 完成综合实战：命令行通讯录程序（建库、增、删、改、查、模糊搜索、事务批量导入）；
- 了解业界现成方案（SQLiteCpp 等）与自研封装的取舍。

本章代码需要 **C++17**（用到 `std::optional` 与 `std::string_view`）。

## 8.1 RAII：把 open/close 变成构造/析构

C API 的三大痛点——手动配对释放、错误路径资源泄漏、异常中断资源泄漏——在 C++ 里用 RAII 一次性解决：

```cpp
class Database {
public:
    explicit Database(const char *path) {
        if (sqlite3_open(path, &db_) != SQLITE_OK) {
            std::string msg = db_ ? sqlite3_errmsg(db_) : "open failed";
            sqlite3_close_v2(db_);
            throw std::runtime_error("打开数据库失败: " + msg);
        }
        sqlite3_busy_timeout(db_, 3000);
        exec("PRAGMA journal_mode = WAL;"
             "PRAGMA foreign_keys = ON;"
             "PRAGMA synchronous = NORMAL;");
    }
    ~Database() { sqlite3_close_v2(db_); }

    Database(const Database&) = delete;             /* 连接不可复制 */
    Database& operator=(const Database&) = delete;

    sqlite3 *handle() const { return db_; }

    void exec(const char *sql) {                    /* DDL/无参语句 */
        char *err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "unknown";
            sqlite3_free(err);
            throw std::runtime_error("exec 失败: " + msg);
        }
    }

private:
    sqlite3 *db_ = nullptr;
};
```

设计要点：

- **构造失败抛异常**，对象要么完整可用要么不存在，杜绝半初始化状态；
- **禁止拷贝**：连接句柄复制会造成 double-close；需要转移所有权时再实现移动构造（本章从简）；
- 析构用 `close_v2`，即使有疏漏的 stmt 也安全延迟关闭。

`Statement` 同理：构造时 `prepare`，析构时 `finalize`，绑定与取值收编为成员函数：

```cpp
class Statement {
public:
    Statement(Database &db, const char *sql) : db_(db.handle()) {
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt_, nullptr) != SQLITE_OK)
            throw std::runtime_error(std::string("prepare 失败: ") + sqlite3_errmsg(db_));
    }
    ~Statement() { sqlite3_finalize(stmt_); }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    /* 链式绑定：st.bind(1, name).bind(2, phone); */
    Statement& bind(int idx, std::string_view v) {
        sqlite3_bind_text(stmt_, idx, v.data(), static_cast<int>(v.size()),
                          SQLITE_TRANSIENT);
        return *this;
    }
    Statement& bind(int idx, sqlite3_int64 v) { sqlite3_bind_int64(stmt_, idx, v); return *this; }
    Statement& bind(int idx, double v)        { sqlite3_bind_double(stmt_, idx, v); return *this; }

    bool step() {                              /* true = 还有一行 */
        int rc = sqlite3_step(stmt_);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE)
            throw std::runtime_error(std::string("step 失败: ") + sqlite3_errmsg(db_));
        return rc == SQLITE_ROW;
    }
    void exec() { (void)step(); }              /* 写语句执行 */

    sqlite3_int64  getInt(int c) const { return sqlite3_column_int64(stmt_, c); }
    std::string getText(int c) const {          /* 留存即拷贝——第 5 章的坑在这里根治 */
        const unsigned char *p = sqlite3_column_text(stmt_, c);
        return p ? reinterpret_cast<const char*>(p) : std::string();
    }
    bool isNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
    sqlite3_int64 lastInsertId() const { return sqlite3_last_insert_rowid(db_); }

private:
    sqlite3 *db_;
    sqlite3_stmt *stmt_ = nullptr;
};
```

注意 `getText` 构造 `std::string` 的那一刻就完成了拷贝——"列值指针只在本行有效"的 C 层陷阱在封装层被消除。

## 8.2 综合实战：命令行通讯录

单文件 `contacts_app.cpp`，功能：`add` / `list` / `search` / `del` / `rename-group` / `import`（事务批量导入 CSV）。完整代码：

```cpp
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sqlite3.h>

/* ---- 8.1 的 Database 与 Statement 两个类，原样放入 ---- */
// ...（此处省略，见 8.1 节源码）...

struct Contact {
    sqlite3_int64 id;
    std::string name, phone, group;
};

class ContactDao {                          /* 数据访问层：SQL 只出现在这里 */
    Database &db_;
public:
    explicit ContactDao(Database &db) : db_(db) {
        db_.exec(
            "CREATE TABLE IF NOT EXISTS contacts ("
            "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name       TEXT    NOT NULL,"
            "  phone      TEXT    NOT NULL UNIQUE,"
            "  group_name TEXT    DEFAULT '朋友',"
            "  created_at DATETIME DEFAULT (datetime('now','localtime')));");
    }

    sqlite3_int64 add(const std::string &name, const std::string &phone,
                      const std::string &group = "朋友") {
        Statement st(db_, "INSERT INTO contacts(name, phone, group_name) VALUES(?,?,?)");
        st.bind(1, name).bind(2, phone).bind(3, group);
        try {
            st.exec();
        } catch (const std::runtime_error &e) {
            if (std::string(e.what()).find("UNIQUE") != std::string::npos)
                throw std::runtime_error("电话号码已存在: " + phone);
            throw;
        }
        return st.lastInsertId();
    }

    std::vector<Contact> search(const std::string &kw) {
        Statement st(db_, "SELECT id,name,phone,IFNULL(group_name,'') "
                          "FROM contacts WHERE name LIKE ? OR phone LIKE ? "
                          "ORDER BY id");
        std::string pat = "%" + kw + "%";
        st.bind(1, pat).bind(2, pat);
        std::vector<Contact> out;
        while (st.step())
            out.push_back({st.getInt(0), st.getText(1), st.getText(2), st.getText(3)});
        return out;
    }

    bool remove(sqlite3_int64 id) {
        Statement st(db_, "DELETE FROM contacts WHERE id = ?");
        st.bind(1, id).exec();
        return sqlite3_changes(db_.handle()) > 0;
    }

    /* 事务批量导入：单条失败全部回滚（第 7 章） */
    void importCsv(const std::string &path) {
        std::ifstream in(path);
        if (!in) throw std::runtime_error("打不开文件: " + path);
        std::string line;
        std::getline(in, line);                       /* 跳过表头 */
        db_.exec("BEGIN");
        try {
            while (std::getline(in, line)) {
                std::stringstream ss(line);
                std::string name, phone;
                std::getline(ss, name, ',');
                std::getline(ss, phone, ',');
                if (name.empty() || phone.empty()) continue;
                add(name, phone);
            }
            db_.exec("COMMIT");
        } catch (...) {
            db_.exec("ROLLBACK");
            throw;
        }
    }
};

static void print(const std::vector<Contact> &v) {
    for (const auto &c : v)
        std::cout << "[" << c.id << "] " << c.name << "  " << c.phone
                  << "  (" << c.group << ")\n";
    std::cout << "共 " << v.size() << " 条\n";
}

int main(int argc, char **argv) {
    try {
        Database db("contacts.db");
        ContactDao dao(db);
        std::string cmd = argc > 1 ? argv[1] : "list";

        if (cmd == "add" && (argc == 4 || argc == 5)) {
            std::string g = argc == 5 ? argv[4] : "朋友";
            std::cout << "已插入 id=" << dao.add(argv[2], argv[3], g) << "\n";
        } else if (cmd == "list" || cmd == "search") {
            print(dao.search(cmd == "search" && argc > 2 ? argv[2] : ""));
        } else if (cmd == "del" && argc == 3) {
            std::cout << (dao.remove(std::stoll(argv[2])) ? "已删除\n" : "无此 id\n");
        } else if (cmd == "import" && argc == 3) {
            dao.importCsv(argv[2]);
            std::cout << "导入完成\n";
        } else {
            std::cout << "用法: app add <名> <号> [分组] | list | search <关键词> "
                         "| del <id> | import <csv>\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "错误: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
```

编译（把 8.1 的两个类贴到省略处）：

```text
g++ -std=c++17 -O2 contacts_app.cpp sqlite3.c -o contacts_app.exe
contacts_app.exe add 张三 13800000001 同事
contacts_app.exe list
contacts_app.exe search 张
contacts_app.exe import people.csv
```

架构上值得注意的是 **DAO 分层**：`main` 里没有一行 SQL，所有语句集中在 `ContactDao`——SQL 与业务逻辑的边界清晰，后续换库、加字段都只动一处。这同样是把本书前七章知识（绑定、事务、错误信息分流、IFNULL 处理 NULL）串联起来的落点。

## 8.3 自研还是用轮子

| 方案 | 代表 | 适合 |
| --- | --- | --- |
| 直接用 C API | `sqlite3.h` | 学习期、极简依赖项目；C 项目唯一选择 |
| 轻量自研封装 | 本章 200 行 | 中小项目，需求可控，无第三方依赖 |
| 第三方库 | [SQLiteCpp](https://github.com/SRombauts/SQLiteCpp)、sqlite_modern_cpp 等 | 想要现成的 RAII、事务守卫、迭代器语法 |

SQLiteCpp 的写法长这样，与本章封装神似：

```cpp
SQLite::Database db("contacts.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
SQLite::Statement query(db, "SELECT id, name FROM contacts WHERE group_name = ?");
query.bind(1, "同事");
while (query.executeStep())
    std::cout << query.getColumn(0) << " " << query.getColumn(1) << "\n";
```

建议路径：**先写一遍本章的迷你封装理解每个 API 的职责，再在真实项目里按团队情况选型**。理解了裸 API，任何封装在你眼里都是透明的。

## 8.4 结课检查清单

对照打勾，全部通过即完成本书：

- [ ] 能用 CLI 建库、查表结构、导入导出（第 2 章）；
- [ ] 能手写含约束的建表语句与常用 CRUD（第 3 章）；
- [ ] 能在三种编译方式下跑通第一个 C 程序（第 4 章）；
- [ ] 说得出 exec 与 prepare/step 各自的适用场景（第 5 章）；
- [ ] 能解释 SQLITE_TRANSIENT 的含义与防注入原理（第 6 章）；
- [ ] 能用事务把批量写入提速两个数量级并正确回滚（第 7 章）；
- [ ] 能用 RAII 封装出资源安全的 C++ 访问层（第 8 章）。

## 动手练习

1. 给 `Statement` 补上 `bind(int, std::nullptr_t)` 重载实现绑定 NULL，并在 `ContactDao` 中增加 `clearGroup(id)`（把分组置 NULL）方法验证；
2. 给 `ContactDao` 加 `updatePhone(id, phone)`，处理"新号码与他人重复"的 UNIQUE 冲突并给出友好错误；
3. 扩展 `importCsv`：CSV 每行带分组列（三列），分组为空字符串时入库为默认值 `'朋友'` 而不是空串（提示：绑定前判断）；
4. 进阶：实现 `Database` 的移动构造与移动赋值，保证移动后源对象 `handle()` 为空指针。

<details>
<summary>参考答案（练习 1 与练习 4）</summary>

```cpp
/* 练习 1：绑定 NULL */
Statement& bind(int idx, std::nullptr_t) {
    sqlite3_bind_null(stmt_, idx);
    return *this;
}

void ContactDao::clearGroup(sqlite3_int64 id) {
    Statement st(db_, "UPDATE contacts SET group_name = NULL WHERE id = ?");
    st.bind(1, id).exec();
}

/* 练习 4：移动语义——接管资源后置空源 */
class Database {
public:
    Database(Database &&o) noexcept : db_(o.db_) { o.db_ = nullptr; }
    Database& operator=(Database &&o) noexcept {
        if (this != &o) { sqlite3_close_v2(db_); db_ = o.db_; o.db_ = nullptr; }
        return *this;
    }
    /* 其余同 8.1。析构处 sqlite3_close_v2(nullptr) 是安全调用，无需判空 */
};
```

练习 4 的验收要点：移动后源对象析构不得关闭新持有者的连接（通过置空源指针保证）；自赋值要防护；`noexcept` 让容器（如 `std::vector<Database>`）在扩容时走移动而非拷贝（拷贝已被 delete，否则编译失败）。

</details>

## 本章小结

- RAII 封装三大收益：自动配对释放、异常安全、`getText` 一步拷贝根治悬垂指针；
- 禁拷贝、允移动是数据库连接类的标准形态；错误统一抛 `std::runtime_error` 携带 errmsg；
- DAO 分层让 SQL 集中于一处，业务层零 SQL；
- 自研 200 行封装足以应对中小项目；理解裸 API 后，SQLiteCpp 等轮子即插即用。

## 结语

到这里，你已经走完"工具 → SQL → C API → 工程实践"的完整链路。SQLite 的世界远不止于此——全文检索（FTS5）、JSON 函数、备份 API、多线程进阶——它们都建立在你现在掌握的地基之上，需要时翻阅 [官方文档](https://www.sqlite.org/docs.html) 即可平滑延伸。下一个建议：把你手头一个真实的小需求（配置存储、图纸元数据、操作日志）迁移到 SQLite，用本书的封装跑一遍，知识就真正长在你手上了。
