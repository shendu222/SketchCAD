# 第 2 章 sqlite3 命令行工具快速上手

## 本章目标

- 掌握 `sqlite3` 交互式命令行的常用点命令（dot commands）；
- 亲手创建 `contacts.db` 并录入第一批数据；
- 学会用 CLI 导出查询结果、导入 CSV、查看表结构。

`sqlite3` 命令行工具是学习 SQLite 最好的沙箱：任何 SQL 在写进 C 代码之前，都应该先在这里跑通。本章所有操作对后续章节直接复用。

## 2.1 启动与退出

```text
$ sqlite3 contacts.db
SQLite version 3.50.0
Enter ".help" for usage hints.
sqlite>
```

- `sqlite3` 不带参数 → 打开一个**内存数据库**，退出即消失，适合做 SQL 实验；
- `sqlite3 contacts.db` → 打开（不存在则创建）文件数据库；
- `sqlite3 contacts.db "SELECT count(*) FROM contacts;"` → 直接执行一条 SQL 后退出，适合脚本调用；
- 退出：`.quit`（或 Ctrl+D / Ctrl+Z 后回车）。

> **注意**：数据库文件在第一条建表语句执行前并不会真正落盘，只 `sqlite3 contacts.db` 就退出的话，目录里可能什么都不留。

## 2.2 必会的点命令

点命令以 `.` 开头、**不需要分号**，SQL 语句以 `;` 结尾——这是初学者最常见的混淆点。

| 命令 | 作用 |
| --- | --- |
| `.open 文件名` | 打开/切换数据库文件（会话中途换库） |
| `.tables` | 列出所有表 |
| `.schema 表名` | 查看建表语句（不加表名则列出全部） |
| `.headers on` | 查询结果显示列名 |
| `.mode column` / `.mode box` | 对齐显示（`box` 需要 3.33+，带表格线更好看） |
| `.width 5 15 15` | 手动指定每列宽度 |
| `.import 文件.csv 表名` | 导入 CSV（先 `.mode csv`） |
| `.output 文件名` / `.once 文件名` | 把后续输出（或下一条命令输出）写入文件 |
| `.dump` | 导出整个数据库为 SQL 文本（备份利器） |
| `.databases` | 显示当前打开的数据库与文件路径 |
| `.help` | 全部命令列表 |

### 手把手：建库三分钟

```text
sqlite> .open contacts.db
sqlite> CREATE TABLE contacts (
   ...>     id      INTEGER PRIMARY KEY AUTOINCREMENT,
   ...>     name    TEXT    NOT NULL,
   ...>     phone   TEXT    NOT NULL UNIQUE,
   ...>     group_name TEXT DEFAULT '朋友',
   ...>     created_at  DATETIME DEFAULT (datetime('now','localtime'))
   ...> );
sqlite> .tables
contacts
sqlite> .schema contacts
CREATE TABLE contacts ( ...刚输入的建表语句... );
sqlite> .headers on
sqlite> .mode box
sqlite> INSERT INTO contacts(name, phone, group_name) VALUES ('张三', '13800000001', '同事');
sqlite> INSERT INTO contacts(name, phone, group_name) VALUES ('李四', '13800000002');
sqlite> SELECT * FROM contacts;
┌────┬──────┬─────────────┬────────────┬─────────────────────┐
│ id │ name │    phone    │ group_name │     created_at      │
├────┼──────┼─────────────┼────────────┼─────────────────────┤
│ 1  │ 张三 │ 13800000001 │ 同事       │ 2026-10-17 09:30:00 │
│ 2  │ 李四 │ 13800000002 │ 朋友       │ 2026-10-17 09:30:05 │
└────┴──────┴─────────────┴────────────┴─────────────────────┘
```

说明三点：

1. 多行 SQL 中，续行提示符是 `...>`，语句以 `;` 才会执行；
2. 第二条 INSERT 没给 `group_name`，落库时自动取默认值 `'朋友'`；`created_at` 同理由默认表达式填充；
3. `.mode box` + `.headers on` 是交互查询的黄金组合，建议每次启动都设置（也可以写进 `~/.sqliterc` 一劳永逸）。

## 2.3 实用的导入导出

### 把查询结果导出为 CSV

```text
sqlite> .headers on
sqlite> .mode csv
sqlite> .once contacts_backup.csv
sqlite> SELECT * FROM contacts;
sqlite> .mode box
```

`.once` 只影响下一条 SQL 的输出，之后自动恢复屏幕输出。

### 从 CSV 导入

假设有 `people.csv`（第一行是列名）：

```csv
name,phone
王五,13800000003
赵六,13800000004
```

```text
sqlite> .mode csv
sqlite> .import --csv --skip 1 people.csv contacts
sqlite> SELECT id, name FROM contacts;
```

新版本（3.32+）的 `.import` 支持 `--csv --skip 1` 跳过表头行；老版本需要先 `.mode csv` 再 `.import`，表头会作为数据行混入，需手动清理。导入后务必抽查。

### 整库备份：dump 与恢复

```text
sqlite> .output backup.sql
sqlite> .dump
sqlite> .output stdout
```

恢复则把 `backup.sql` 重放一遍：

```text
$ sqlite3 new.db < backup.sql
```

`.dump` 生成的是纯 SQL 文本，跨版本、跨平台、可用任意文本工具检查——小型数据库用它做定时备份足够了（在线上运行的大库请改用官方备份 API，超出本书范围）。

## 2.4 交互小技巧

- **多语句一行**：`INSERT ...; INSERT ...;` 中间用分号隔开即可；
- **注释**：SQL 里 `--` 后是单行注释，C 风格 `/* */` 也支持；
- **执行系统命令**：`.shell dir`（Windows）或 `.shell ls`（Linux）不必退出 CLI；
- **查看函数与 PRAGMA**：`SELECT sqlite_version();`、`PRAGMA table_info(contacts);`（后者以表格列出某张表所有列的定义，比 `.schema` 更机器友好）。

## 动手练习

1. 新建 `test.db`，建一张 `notes(id INTEGER PRIMARY KEY, content TEXT, ts DATETIME DEFAULT (datetime('now','localtime')))` 表，插入 3 条记录；
2. 分别用 `.mode box`、`.mode csv`、`.mode json`（3.33+）查看 `SELECT * FROM notes` 的输出差异；
3. 用 `.dump` 把 `contacts.db` 导出到 `backup.sql`，然后用 `sqlite3 restored.db < backup.sql` 恢复一份，`.tables` 验证两库一致；
4. 执行 `INSERT INTO contacts(name, phone) VALUES ('张三', '13900000009');`，观察并解释报错。

<details>
<summary>参考答案（练习 4）</summary>

这条语句**大概率会成功**——`UNIQUE` 约束在 `phone` 列上，与姓名无关：库里只有 `13800000001`，而 `13900000009` 不存在，同名不同号完全允许。想亲手触发约束错误，把号码改成已存在的值即可：

```text
sqlite> INSERT INTO contacts(name, phone) VALUES ('张三', '13800000001');
Parse error: UNIQUE constraint failed: contacts.phone
```

两个收获：① 报错信息会精确到列名（`contacts.phone`），排查约束冲突时以它为准；② 约束在**落库前**检查，违规数据不会写入半截记录——这正是第 7 章"事务原子性"在日常最常见的体现。程序中这个错误对应错误码 19（`SQLITE_CONSTRAINT`），第 7 章会讲如何捕获。

</details>

## 本章小结

- 点命令无分号、SQL 有分号，这是 CLI 第一坑；
- `.open` 打开/建库，`.tables` / `.schema` / `PRAGMA table_info` 查看结构，`.headers on` + `.mode box` 是查询显示最佳拍档；
- `.import --csv --skip 1` 导入 CSV，`.dump` 导出全库 SQL 文本做备份与迁移；
- `UNIQUE` 约束违反时 CLI 直接报 `UNIQUE constraint failed`，程序里它表现为一个错误码（第 7 章）。

## 下一章预告

现在你能用 CLI 建库插数据了，但对 SQL 本身可能还是"照抄能跑"的状态。第 3 章把日常开发 90% 用到的 SQL——建表约束、插入、查询、更新、删除、排序分页——系统过一遍，为第 4 章进入 C 编程打好地基。
