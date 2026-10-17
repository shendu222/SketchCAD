# 第 5 章 执行 SQL 的两种方式

## 本章目标

- 吃透 `sqlite3_exec` 的回调机制与适用边界；
- 掌握预编译四步曲 `prepare → bind → step → finalize` 的完整流程；
- 理解"为什么生产代码优先用预编译语句"：安全、类型、性能三重理由。

## 5.1 路线一：sqlite3_exec（便捷式）

```c
int sqlite3_exec(
    sqlite3 *db,              /* 连接句柄 */
    const char *sql,          /* 一条或多条分号分隔的 SQL */
    sqlite3_callback cb,      /* 每查到一行调用一次（无结果集可传 NULL） */
    void *user_data,          /* 原样传给回调的指针 */
    char **errmsg             /* 出参：错误信息拷贝，需 sqlite3_free */
);
```

### 手把手：用回调接收查询结果

```c
#include <stdio.h>
#include <sqlite3.h>

/* 参数：user_data、列数、列名数组、列值数组（均为只读） */
static int print_row(void *user, int argc, char **argv, char **col_name)
{
    (void)user;
    for (int i = 0; i < argc; i++) {
        printf("%s = %s%s", col_name[i],
               argv[i] ? argv[i] : "NULL",   /* 列值为 NULL 时 argv[i] 是 NULL 指针！ */
               i < argc - 1 ? ", " : "\n");
    }
    return 0;   /* 返回非 0 会中止执行，exec 返回 SQLITE_ABORT */
}

int main(void)
{
    sqlite3 *db;
    if (sqlite3_open("contacts.db", &db) != SQLITE_OK) {
        fprintf(stderr, "打开失败: %s\n", sqlite3_errmsg(db));
        return sqlite3_close_v2(db);
    }

    char *errmsg = NULL;
    int rc = sqlite3_exec(db, "SELECT id, name, phone FROM contacts;",
                          print_row, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "查询失败: %s\n", errmsg);
        sqlite3_free(errmsg);
    }

    sqlite3_close_v2(db);
    return rc;
}
```

`exec` 的优点是一次调用走完"编译+执行+取行"，SQL 里没有用户输入时（建表、迁移脚本、写死的统计语句）它干净利落。但它有两条硬伤：

1. **回调风格反直觉**，回调里中断查询、传递错误都很别扭；
2. **无法绑定参数**——要拼用户输入只能 `sprintf` 拼字符串，这正是 SQL 注入的温床（第 6 章反面示范）。

## 5.2 路线二：prepare → step → finalize（预编译式）

这是 SQLite 官方文档的"正经姿势"，标准四步：

```text
sqlite3_prepare_v2()   把 SQL 编译成 stmt（语法在此检查）
        │
sqlite3_bind_xxx()     把参数填进 ? 占位符（第 6 章专讲）
        │
sqlite3_step()         推进一步：查到一行返回 SQLITE_ROW；跑完返回 SQLITE_DONE
        │  （循环 step 直到非 SQLITE_ROW）
sqlite3_finalize()     释放语句对象
```

### 手把手：用 step 循环取行

```c
#include <stdio.h>
#include <sqlite3.h>

int main(void)
{
    sqlite3 *db;
    if (sqlite3_open("contacts.db", &db) != SQLITE_OK) {
        fprintf(stderr, "打开失败: %s\n", sqlite3_errmsg(db));
        return sqlite3_close_v2(db);
    }

    const char *sql = "SELECT id, name, phone FROM contacts WHERE group_name = '朋友'";
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);   /* -1 = 自动算长度 */
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL 错误: %s\n", sqlite3_errmsg(db));
        sqlite3_close_v2(db);
        return 1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int    id    = sqlite3_column_int(stmt, 0);
        const unsigned char *name  = sqlite3_column_text(stmt, 1);
        const unsigned char *phone = sqlite3_column_text(stmt, 2);
        printf("[%d] %s  %s\n", id, name, phone);   /* 列值指针仅在本行有效，随用随取 */
    }

    if (rc != SQLITE_DONE) {                       /* 循环出来不是 DONE 就是出错 */
        fprintf(stderr, "执行出错: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);                        /* 有 prepare 必有 finalize */
    sqlite3_close_v2(db);
    return rc == SQLITE_DONE ? 0 : 1;
}
```

关键细节：

- **`prepare_v2` 的第 4 个参数 `pzTail`**：传入 SQL 含多条语句时，返回第一条之后的剩余文本指针；单条语句传 `NULL` 即可。永远用 `_v2` 后缀版本，错误码更全（旧版 `prepare` 已是兼容残留）；
- **`step` 的返回值驱动一切**：`SQLITE_ROW`（100）= 有一行可取；`SQLITE_DONE`（101）= 执行完毕；其他 = 出错或被中断；
- **列下标从 0 开始**，顺序与 SELECT 里的列顺序一致；
- **`sqlite3_column_text` 返回的指针生命周期只到下一次 step/finalize**——要留存必须立刻拷贝（`strcpy`/构造 `std::string`）。这是新手内存相关崩溃的头号来源。

### 执行写语句（INSERT/UPDATE/DELETE）

写语句没有结果行，step 一次就完：

```c
const char *sql = "UPDATE contacts SET phone = '13911111111' WHERE id = 1";
sqlite3_stmt *stmt;
sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
if (sqlite3_step(stmt) != SQLITE_DONE) { /* 出错处理 */ }
sqlite3_finalize(stmt);

/* 两个立即可用的伴随信息： */
sqlite3_int64 n = sqlite3_changes(db);            /* 最近语句影响的行数 */
sqlite3_int64 id = sqlite3_last_insert_rowid(db); /* 最近 INSERT 生成的 rowid */
```

## 5.3 两条路线怎么选

| 场景 | 推荐 | 理由 |
| --- | --- | --- |
| 建表、DDL、迁移脚本 | `exec` | 无参数、无结果处理需求 |
| 任何带用户输入的 SQL | `prepare + bind` | 绑定参数天然免疫注入 |
| 循环内反复执行的同类 SQL | `prepare + reset` | 复用编译结果，性能数倍提升 |
| 需要逐行精细处理结果 | `prepare/step` | 回调式难以优雅表达 |
| 一次性小工具脚本 | `exec` | 代码最短 |

`reset` 复用示例（同一语句跑 N 遍只编译一次）：

```c
sqlite3_prepare_v2(db, "INSERT INTO contacts(name, phone) VALUES(?, ?)", -1, &stmt, NULL);
for (int i = 0; i < 1000; i++) {
    sqlite3_bind_text(stmt, 1, names[i], -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, phones[i], -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);        /* 执行 */
    sqlite3_reset(stmt);       /* 回到可重新绑定的初始态（绑定值默认保留） */
}
sqlite3_finalize(stmt);
```

## 5.4 为什么生产代码优先预编译语句

1. **安全**：参数走绑定通道而非字符串拼接，用户输入永远被当作**数据**而不是 SQL 片段（第 6 章正面展开）；
2. **类型可靠**：`bind_int/bind_text` 按类型传输，文本中的引号、反斜杠、中文统统原样入库，无需转义；
3. **性能**：编译一次反复 `reset` 执行；批量插入配合事务（第 7 章）可把上万行从分钟级压到毫秒级。

## 动手练习

1. 用 `exec` + 回调实现"统计通讯录总人数"（提示：`SELECT COUNT(*)`，在回调里取 `argv[0]`）；
2. 用 `prepare/step` 实现同样功能，对比两种写法的代码量与可控性；
3. 把 5.2 的查询程序改成：先 `SELECT group_name, COUNT(*) FROM contacts GROUP BY group_name`，在 step 循环里打印每组人数；
4. 写一个循环插入 1000 行的 `INSERT` 程序（先用 `?` 占位、每轮 `reset`），统计耗时（`clock()`），为第 7 章事务实验做基线。

<details>
<summary>参考答案（练习 2 关键代码）</summary>

```c
sqlite3_stmt *stmt;
sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM contacts", -1, &stmt, NULL);
int total = -1;
if (sqlite3_step(stmt) == SQLITE_ROW)
    total = sqlite3_column_int(stmt, 0);
sqlite3_finalize(stmt);
printf("总人数: %d\n", total);
```

对比结论：本例只有一列一行，两种方式代码量相近；但一旦涉及"列可能为 NULL""需要提前中断""结果要转成结构体"，回调式立刻变得笨拙——`column` 系列函数直接在循环里取值，NULL 自动返回 0/空指针，逻辑与控制流都在一个函数里，可读性和可维护性明显更好。这就是官方文档通篇用 `prepare/step` 示范的原因。

</details>

## 本章小结

- `exec` = 编译+执行+回调一步走，适合无参 SQL 与 DDL；`errmsg` 出参要 `sqlite3_free`；
- 预编译四步曲 `prepare_v2 → bind → step → finalize`，`step` 用 `SQLITE_ROW/SQLITE_DONE` 驱动循环；
- 列下标从 0 起；`column_text` 指针只在本行有效，留存必须拷贝；
- `sqlite3_changes` 取影响行数，`sqlite3_last_insert_rowid` 取新插入行的主键；
- 同语句反复执行用 `reset` 复用，编译只付一次成本。

## 下一章预告

四步曲中的 `bind` 一直是"先用再说"，第 6 章正式讲清参数绑定：三种占位符写法、五种 bind 函数、`SQLITE_TRANSIENT` 到底是什么，以及查询取值的类型处理——全书最实用的一章。
