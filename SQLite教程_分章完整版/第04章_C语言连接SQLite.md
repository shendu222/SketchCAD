# 第 4 章 C 语言连接 SQLite：第一个程序

## 本章目标

- 理解 SQLite C API 的核心对象：`sqlite3`（连接句柄）与 `sqlite3_stmt`（语句对象）；
- 写出第一个完整 C 程序：打开数据库 → 执行一条建表 SQL → 关闭；
- 用命令行（MSVC / MinGW）和 CMake 两种方式编译链接成功；
- 掌握 `sqlite3_open` / `sqlite3_close` 的细节与常见坑。

## 4.1 两个核心对象

整个 SQLite C API 围绕两个不透明结构体展开，你永远只通过指针和函数操作它们：

| 对象 | 角色 | 生命周期函数 |
| --- | --- | --- |
| `sqlite3` | **数据库连接句柄**，代表"一个打开的数据库文件" | `sqlite3_open` → … → `sqlite3_close` |
| `sqlite3_stmt` | **预编译语句**，代表"一条正在执行的 SQL" | `sqlite3_prepare_v2` → … → `sqlite3_finalize` |

后文的口诀：**open 配 close，prepare 配 finalize**，两者都必须成对出现，漏了就是资源泄漏或崩溃。

## 4.2 第一个程序

`first.c`——打开（或创建）`contacts.db`，如果表不存在就建表，然后关闭：

```c
#include <stdio.h>
#include <sqlite3.h>

int main(void)
{
    sqlite3 *db = NULL;
    int rc = sqlite3_open("contacts.db", &db);
    if (rc != SQLITE_OK) {
        /* 注意：即使 open 失败，db 也可能非 NULL，close 仍需调用 */
        fprintf(stderr, "无法打开数据库: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS contacts ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT    NOT NULL,"
        "  phone      TEXT    NOT NULL UNIQUE,"
        "  group_name TEXT    DEFAULT '朋友',"
        "  created_at DATETIME DEFAULT (datetime('now','localtime'))"
        ");";

    char *errmsg = NULL;
    rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "建表失败: %s\n", errmsg);
        sqlite3_free(errmsg);          /* errmsg 指向的内存要手动释放 */
        sqlite3_close(db);
        return 1;
    }

    printf("数据库已就绪，SQLite 版本: %s\n", sqlite3_libversion());
    sqlite3_close(db);
    return 0;
}
```

逐点解读：

1. **`sqlite3_open`（文件名, &db）**：文件存在则打开，不存在则**创建**——和 CLI 行为一致。想改变这个行为用 `sqlite3_open_v2`：

   ```c
   int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE; /* 默认行为 */
   int flags_ro = SQLITE_OPEN_READONLY;                    /* 只读打开，不存在则失败 */
   rc = sqlite3_open_v2("contacts.db", &db, flags_ro, NULL);
   ```

   两个特殊文件名：`:memory:` 打开纯内存数据库（速度极快，测试首选）；空字符串 `""` 创建匿名临时库，连接关闭即消失。

2. **`sqlite3_exec`**：一步执行 SQL 的便捷函数（可一次跑多条分号分隔的语句），本章先用它完成建表，第 5 章专门剖析它和 `prepare/step` 的分工。

3. **`sqlite3_errmsg(db)`**：返回最近一次错误的**英文描述**，指向的内存归 SQLite 所有，**不要 free 它**；而 `exec` 传出的 `errmsg` 是**拷贝**，必须 `sqlite3_free`。这两条规则最容易搞反。

4. **`sqlite3_close(db)`**：归还连接。若还有未 finalize 的 stmt，老接口会返回 `SQLITE_BUSY` 拒绝关闭；`sqlite3_close_v2` 则宽容地"延迟到安全时再关"。新手统一用 `sqlite3_close_v2` 最省心。

## 4.3 编译与链接

把第 1 章下载的 `sqlite3.c`、`sqlite3.h` 与 `first.c` 放同一目录（下文均如此假设）。

### 方式一：命令行直编

**MinGW-w64（gcc）/ clang：**

```text
gcc first.c sqlite3.c -I. -o first.exe
./first.exe
```

**MSVC（在 "x64 Native Tools Command Prompt" 中）：**

```text
cl first.c sqlite3.c /I. /Fe:first.exe
first.exe
```

> **为什么要 `-I.`（MSVC 为 `/I.`）？** 代码里写的是 `#include <sqlite3.h>`，尖括号形式只搜索系统包含路径，不搜索当前目录；`-I.` 显式把当前目录加入搜索路径。（若把尖括号改成 `#include "sqlite3.h"` 则会搜索源文件所在目录，可省略此选项，两种写法任选其一并保持全书一致。）

编译 `sqlite3.c` 需要几十秒属于正常现象。链接阶段零配置——没有导入库、没有运行时 DLL 依赖，一切都在这一个 `.c` 里。

> **链接系统安装版时**（Linux `libsqlite3-dev` 等）则改为：`gcc first.c -lsqlite3 -o first`，此时不需要 `sqlite3.c`，只要 `sqlite3.h`。

### 方式二：CMake（多文件项目推荐）

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.16)
project(sqlite_demo C)

# 把引擎编成静态库目标：源码不变时增量编译，只在首次付出编译成本
add_library(sqlite3 STATIC sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(first first.c)
target_link_libraries(first PRIVATE sqlite3)
```

构建：

```text
cmake -B build
cmake --build build
./build/first.exe        # Windows
```

把引擎做成 `add_library` 的好处：改业务代码重编译时，`sqlite3.c` 不会重新编译，秒级完成链接。

### 编译选项建议（可选进阶）

- `SQLITE_ENABLE_FTS5` / `SQLITE_ENABLE_RTREE`：启用全文检索/空间索引扩展；
- `SQLITE_THREADSAFE=1`：多线程安全（默认开启，第 7 章详述）；
- MSVC 下大规模编译可加 `/utf-8` 避免 `sqlite3.c` 在某些代码页下的警告。

CMake 中写法：`target_compile_definitions(sqlite3 PRIVATE SQLITE_ENABLE_FTS5)`。

## 4.4 常见坑速查

| 现象 | 原因与解法 |
| --- | --- |
| 链接错误 `undefined reference to sqlite3_open` | 忘了把 `sqlite3.c` 加入编译/链接，或系统方式忘了 `-lsqlite3` |
| MSVC 报 C4996 警告刷屏 | 加编译选项 `/wd4996` 或忽略（sqlite3.c 官方如此） |
| 程序一跑就"数据库锁定" | 另一个程序（多半是开着的 CLI 或 DB Browser）占着写锁，见第 7 章 |
| 中文乱码 | Windows 控制台代码页问题，先 `chcp 65001`；数据库本身按 UTF-8 存储无此问题 |
| open 返回 OK 但文件没出现 | 只 open 不执行任何写语句，SQLite 不落盘，属正常行为 |

## 动手练习

1. 修改 `first.c`：用 `sqlite3_open_v2` 以只读方式打开 `contacts.db`，随后再执行 `INSERT` 语句，观察报错信息（`exec` 的 errmsg）；
2. 给程序加上命令行参数：`./first 数据库名`，把 `argv[1]` 传给 `sqlite3_open`；
3. 打印 `sqlite3_libversion()` 与 `sqlite3_compileoption_used("SQLITE_ENABLE_FTS5")`，确认当前编译选项状态；
4. 故意把 SQL 写错（如 `CREAT TABLE ...`），运行观察 `errmsg` 输出，体会"错误信息比错误码更能定位问题"。

<details>
<summary>参考答案（练习 1 关键代码）</summary>

```c
int rc = sqlite3_open_v2("contacts.db", &db, SQLITE_OPEN_READONLY, NULL);
if (rc != SQLITE_OK) { /* ... */ }

char *errmsg = NULL;
rc = sqlite3_exec(db, "INSERT INTO contacts(name, phone) VALUES('测试','13000000000');",
                  NULL, NULL, &errmsg);
if (rc != SQLITE_OK) {
    printf("插入失败(%d): %s\n", rc, errmsg);  /* attempt to write a readonly database */
    sqlite3_free(errmsg);
}
```

只读连接上执行写语句返回 `SQLITE_READONLY`（8），错误信息为 `attempt to write a readonly database`。这提示了一个实践：**打开配置类数据库时主动用只读标志，可以从根上杜绝误写**。

</details>

## 本章小结

- `sqlite3` 是连接句柄（open/close 配对），`sqlite3_stmt` 是语句对象（prepare/finalize 配对）；
- `sqlite3_open` 不存在则创建；`:memory:` 是内存库；只读打开用 `sqlite3_open_v2` + `SQLITE_OPEN_READONLY`；
- `sqlite3_exec` 适合一次执行无参 SQL；它传出的 `errmsg` 要 `sqlite3_free`，而 `sqlite3_errmsg()` 返回的字符串不能 free；
- 编译即"把 `sqlite3.c` 一起编进来"；CMake 下用 `add_library(sqlite3 STATIC sqlite3.c)` 实现增量编译。

## 下一章预告

`sqlite3_exec` 只是冰山一角。第 5 章讲清执行 SQL 的两条路线——`exec` 回调式与 `prepare/bind/step/finalize` 预编译式——什么时候用哪个，以及为什么生产代码几乎总是后者。
