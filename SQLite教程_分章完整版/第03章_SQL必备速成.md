# 第 3 章 SQL 必备速成

## 本章目标

- 看懂并会写日常开发 90% 场景的 SQL：建表（含约束）、INSERT、SELECT、UPDATE、DELETE；
- 掌握 WHERE 条件、ORDER BY 排序、LIMIT/OFFSET 分页、LIKE 模糊匹配；
- 理解 SQLite 的类型亲和（Type Affinity）与几个和 MySQL 不同的方言差异。

本章刻意不贪多：JOIN、聚合进阶、窗口函数等主题只点到为止，先保证核心闭环熟练。

## 3.1 建表与约束

回顾通讯录表，逐行拆解每处约束的作用：

```sql
CREATE TABLE contacts (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,  -- ①主键自增
    name    TEXT    NOT NULL,                   -- ②非空
    phone   TEXT    NOT NULL UNIQUE,            -- ③非空 + 全表唯一
    group_name TEXT DEFAULT '朋友',             -- ④缺省值
    created_at  DATETIME DEFAULT (datetime('now','localtime')) -- ⑤表达式默认值
);
```

① `PRIMARY KEY`：主键唯一标识一行。SQLite 特性：`INTEGER PRIMARY KEY` 就是表内的**行号别名（rowid）**，插入 `NULL` 时自动取当前最大值 +1。`AUTOINCREMENT` 关键字在此基础上保证**永不复用**已删除的 id——普通 `INTEGER PRIMARY KEY` 会回收最大值，绝大多数场景其实不必加 `AUTOINCREMENT`，不加反而更快。

②③ `NOT NULL` / `UNIQUE`：违反约束的插入/更新会直接失败并返回错误（CLI 里的 `UNIQUE constraint failed`）。

④⑤ `DEFAULT`：未提供该列时使用的值，可以是常量，也可以是表达式（如取当前时间）。SQLite 也没有独立的布尔类型，用 `INTEGER` 存 0/1，`CHECK` 约束可以顺手兜底：

```sql
CREATE TABLE items (
    id    INTEGER PRIMARY KEY,
    name  TEXT NOT NULL,
    done  INTEGER NOT NULL DEFAULT 0 CHECK (done IN (0, 1))
);
```

## 3.2 插入：INSERT

```sql
-- 指定列插入（推荐：不依赖列顺序，加列不改代码）
INSERT INTO contacts (name, phone) VALUES ('张三', '13800000001');

-- 多行一次插入（比多次单行快得多）
INSERT INTO contacts (name, phone, group_name) VALUES
    ('李四', '13800000002', '同事'),
    ('王五', '13800000003', '家人');

-- 冲突时的两种处理
INSERT OR IGNORE INTO contacts (name, phone) VALUES ('张三', '13800000001'); -- 违反约束则静默跳过
INSERT OR REPLACE INTO contacts (name, phone) VALUES ('张三', '13900000000'); -- 违反唯一约束则删旧行插新行
```

`INSERT OR REPLACE` 要谨慎：它是"删除整行再插入"，没提供的列会被重置为默认值。

## 3.3 查询：SELECT

```sql
-- 全列（生产代码不建议 *，此处演示）
SELECT * FROM contacts;

-- 投影 + 别名
SELECT name AS 姓名, phone AS 电话 FROM contacts;

-- 条件过滤
SELECT * FROM contacts WHERE group_name = '同事' AND name != '张三';

-- IN / 范围 / 模糊
SELECT * FROM contacts WHERE group_name IN ('同事', '家人');
SELECT * FROM contacts WHERE id BETWEEN 2 AND 10;
SELECT * FROM contacts WHERE name LIKE '张%';   -- % 任意串；_ 单字符
SELECT * FROM contacts WHERE phone LIKE '%0001';

-- 空值判断：必须用 IS NULL，不能用 = NULL
SELECT * FROM contacts WHERE group_name IS NULL;

-- 排序 + 分页（网页列表标配）
SELECT * FROM contacts
ORDER BY created_at DESC        -- DESC 降序，ASC 升序（默认）
LIMIT 20 OFFSET 0;              -- 每页 20 条，第 1 页；OFFSET=(页码-1)*20

-- 聚合与分组
SELECT group_name, COUNT(*) AS cnt FROM contacts
GROUP BY group_name
HAVING cnt >= 2                 -- HAVING 过滤分组结果，WHERE 过滤行
ORDER BY cnt DESC;
```

两处易错：

- `LIKE` 默认对 ASCII 大小写不敏感，但对中文没有"大小写"概念，按字面匹配；需要严格大小写敏感时用 `GLOB` 或 `PRAGMA case_sensitive_like = ON`；
- 分页大数据量时 `OFFSET` 越大越慢，深翻页用"游标法"：`WHERE id < 上页最小id ORDER BY id DESC LIMIT 20`。

**多表 JOIN 最小示例**（点到为止）：

```sql
CREATE TABLE groups (id INTEGER PRIMARY KEY, title TEXT);
CREATE TABLE members (gid INTEGER REFERENCES groups(id), cid INTEGER REFERENCES contacts(id));

SELECT c.name, g.title
FROM members m
JOIN contacts c ON c.id = m.cid
JOIN groups   g ON g.id = m.gid;
```

## 3.4 更新与删除

```sql
UPDATE contacts SET phone = '13911111111' WHERE id = 1;   -- 不带 WHERE = 全表更新，高危！
UPDATE contacts SET group_name = '家人', name = '王五' WHERE phone = '13800000003';

DELETE FROM contacts WHERE id = 5;                        -- 不带 WHERE = 清空全表，高危！
```

**铁律：UPDATE/DELETE 先写 WHERE 再填表名之后的内容，执行前先 SELECT 验证 WHERE 命中的行。** 这条习惯能避免绝大多数"手滑删库"事故。CLI 中还可用 `PRAGMA count_changes = ON` 让每条语句执行后显示影响行数。

## 3.5 SQLite 的类型亲和与方言差异

SQLite 是动态类型：列的"类型"只是**亲和性建议**，实际任何列都能存任何类型。五种亲和规则里记住常用的两条就够：

- 声明为 `INTEGER` 的列优先存整数，`TEXT` 列优先存文本；
- `DATETIME` 在 SQLite 里只是个名字，实际以 `TEXT`（`'2026-10-17 09:30:00'`）、`REAL` 或 `INTEGER`（Unix 时间戳）存储，全靠约定——**用哪一种，整个项目要统一**。本书统一用 `datetime('now','localtime')` 生成的 TEXT 格式。

与 MySQL 常见的方言差异：

| 事项 | MySQL | SQLite |
| --- | --- | --- |
| 自增 | `AUTO_INCREMENT` | `AUTOINCREMENT`（或省略） |
| 布尔 | `TINYINT(1)` / `BOOL` | `INTEGER` 0/1 |
| 字符串拼接 | `CONCAT(a, b)` | `a || b` |
| 当前时间 | `NOW()` | `datetime('now')` |
| 占位符 | `?`（常见） | `?` / `?NNN` / `:name` / `@name`（第 6 章） |
| 大小写 | 库/表名敏感性与配置有关 | 库内标识符不区分大小写 |

## 动手练习

在 `contacts.db` 上完成（先补几条数据）：

1. 查询"同事"分组里按姓名排序的前 3 条记录，只显示 `name` 和 `phone`；
2. 统计每个分组的人数，按人数降序，只保留人数 ≥ 2 的分组；
3. 把所有"家人"分组的联系人电话尾号改为 `0000`（保持前 7 位不变，提示：字符串拼接 `||` 与 `substr`）；
4. 删除 `created_at` 早于 2026 年的记录（提示：字符串比较 `'2026-01-01'` 对标准格式日期直接可用）；
5. 思考：`SELECT * FROM contacts WHERE name = NULL` 结果是什么？为什么？

<details>
<summary>参考答案</summary>

```sql
-- 1
SELECT name, phone FROM contacts
WHERE group_name = '同事' ORDER BY name LIMIT 3;

-- 2
SELECT group_name, COUNT(*) AS cnt FROM contacts
GROUP BY group_name HAVING cnt >= 2 ORDER BY cnt DESC;

-- 3
UPDATE contacts
SET phone = substr(phone, 1, 7) || '0000'
WHERE group_name = '家人';

-- 4
DELETE FROM contacts WHERE created_at < '2026-01-01';

-- 5 结果为空集。NULL 表示"未知"，与任何值（包括另一个 NULL）用 = 比较的结果都是"未知"，
-- 不满足 WHERE 的真值条件，因此一行都不会返回。判断 NULL 必须写 IS NULL / IS NOT NULL。
```

</details>

## 本章小结

- 建表五要素：主键、非空、唯一、默认值、CHECK；`INTEGER PRIMARY KEY` 自动自增，一般无需 `AUTOINCREMENT`；
- 查询三板斧：WHERE 过滤、ORDER BY 排序、LIMIT/OFFSET 分页；NULL 比较必须用 `IS NULL`；
- UPDATE/DELETE 动手前先 SELECT 验证 WHERE，不带 WHERE 就是全表操作；
- SQLite 动态类型：`DATETIME` 本质是文本约定，`||` 拼接字符串，占位符支持 `?` 与命名参数。

## 下一章预告

SQL 基础到位，终于进入正题：第 4 章写第一个 C 程序打开 `contacts.db`，并打通 MSVC/MinGW/CMake 三种编译链接方式——`sqlite3_open` 到 `sqlite3_close`，你的程序从此拥有一个数据库。
