# 第 1 章 初识 SQLite

## 本章目标

- 理解 SQLite 是什么、它和 MySQL/PostgreSQL 这类客户端-服务器数据库的本质区别；
- 明确"什么时候该用 SQLite、什么时候不该"；
- 下载 SQLite 合并源码包（amalgamation），准备好命令行工具和 C/C++ 集成的全部材料；
- 了解 C/C++ 项目集成 SQLite 的三种方式。

## 1.1 SQLite 是什么

SQLite 是一个**嵌入式、零配置、单文件**的 SQL 数据库引擎。它最与众不同的地方在于：**它是一个库，不是一个服务**。

用 MySQL 或 PostgreSQL 时，你的程序是"客户端"，数据库是独立运行的"服务器"，两者通过网络或本机套接字通信；而 SQLite 直接以源码或动态库的形式**编译进你的程序**，数据库就是磁盘上的一个普通文件（比如 `contacts.db`），读写文件就是读写数据库。

| 特性 | SQLite | MySQL / PostgreSQL |
| --- | --- | --- |
| 形态 | 嵌入程序内的 C 库 | 独立服务器进程 |
| 安装部署 | 无需安装，编译进程序 | 需安装、配置、起服务 |
| 数据存储 | 单个跨平台文件 | 专有目录结构 + 配置 |
| 并发写入 | 单写多读（WAL 模式下读写可并发） | 多连接并发写 |
| 适用场景 | 单机应用、嵌入式、配置存储、中小站点 | 高并发多客户端服务端应用 |

几个能帮你建立直觉的事实：

- SQLite 是**世界上部署量最大**的数据库引擎，每一部 Android 和 iPhone 手机、每一款主流浏览器里都有它；
- 一个完整的 SQLite 引擎编译后约 700 KB，无任何外部依赖；
- 单个数据库文件最大支持 281 TB，日常应用远远用不到这个量级。

## 1.2 什么时候用 SQLite

**适合的场景：**

- 桌面软件的本地数据存储（CAD 图纸元数据、配置、历史记录——这正是本书所在 CAD 项目这类程序的需求）；
- 嵌入式设备、IoT；
- 单机小游戏、工具软件的存档；
- 中小型网站的动态内容（SQLite 官方建议：日写入量百万级以内、读多写少的站点）；
- 替代自定义二进制/文本文件格式——很多软件用 SQLite 存缓存、索引、日志，比如 Firefox 和微信的本地消息库；
- 单元测试中的内存数据库（`:memory:`），跑得飞快且无需清理。

**不适合的场景：**

- 多台机器上的多个进程**同时高频写入**同一份数据；
- 数据量大到需要多台服务器分摊负载；
- 需要数据库层面的用户权限管理（SQLite 没有账号体系，权限依赖文件系统）。

经验法则：**数据只属于一台机器上的一个（或少数几个）进程，就用 SQLite；数据要被很多机器同时写，用服务器数据库。**

## 1.3 下载与准备材料

打开官网下载页 <https://www.sqlite.org/download.html>，你需要关注两个包（以 Windows 为例，Linux/macOS 见下文）：

1. **SQLite合并源文件包**：文件名形如 `sqlite-amalgamation-3500000.zip`。包里只有 4 个关键文件：

   - `sqlite3.c` / `sqlite3.h`——整个数据库引擎的合并源码与头文件，这就是 C/C++ 集成所需的全部；
   - `sqlite3ext.h`——写扩展时才用，暂时忽略；
   - `shell.c`——命令行工具的源码。

2. **预编译命令行工具包**：文件名形如 `sqlite-tools-win-x64-3500000.zip`，解压即得 `sqlite3.exe`，免编译。

> **为什么推荐 amalgamation？** SQLite 官方把数百个源文件合并成一个 `sqlite3.c`（约 25 万行），专门为方便集成而维护。把它加进你的项目，就像添加一个普通的 `.c` 文件一样简单，不需要任何第三方包管理器。

**Linux** 用户通常直接用包管理器安装工具（`sudo apt install sqlite3`），而开发集成仍建议下载 amalgamation 以固定版本。**macOS** 自带 `sqlite3` 命令行工具，开发同样推荐下载 amalgamation。

### 手把手：验证命令行工具

解压工具包后，把 `sqlite3.exe` 所在目录加入 PATH（或在所在目录打开终端），执行：

```text
$ sqlite3 --version
3.50.0 2025-05-09 12:15:23 ... (具体输出以你下载的版本为准)
```

能打印出版本号，说明工具就绪。第 2 章我们就用它建第一个数据库。

## 1.4 C/C++ 集成的三种方式

在程序里使用 SQLite，本质都是"拿到 `sqlite3.h` 这个头文件 + 链接上 SQLite 实现"。实现方式有三种，本书统一采用**方式 A**：

### 方式 A：直接编译 amalgamation（推荐）

把 `sqlite3.c` 和 `sqlite3.h` 复制进项目，与你的代码一起编译：

```text
gcc main.c sqlite3.c -o app.exe        # MinGW/clang
cl main.c sqlite3.c /Fe:app.exe        # MSVC
```

- 优点：零依赖、版本可控、跨平台行为一致，最适合桌面软件与嵌入式项目；
- 缺点：`sqlite3.c` 很大，全量编译一次需要几十秒（可用 CMake 目标级缓存规避，见第 4 章）。

### 方式 B：系统包管理器 / vcpkg

- Linux：`sudo apt install libsqlite3-dev`，编译时加 `-lsqlite3`；
- Windows + vcpkg：`vcpkg install sqlite3`，由 CMake 的 `find_package` 接管。

优点是免维护源码；缺点是各机器版本可能不一致，发布桌面软件时要留意目标机器是否装了对应运行库。

### 方式 C：只 amalgamation 中的 `sqlite3.c` 编译成静态库再链接

即先把 `sqlite3.c` 编成 `sqlite3.lib`/`libsqlite3.a`，业务代码只链接库文件。适合多模块大型工程，本书第 8 章 CMake 示例会体现这一思路。

## 1.5 本书的示例数据库

从第 2 章起，全书围绕一个"通讯录"数据库展开，表结构如下（现在看不懂没关系，第 3 章详细讲）：

```sql
CREATE TABLE contacts (
    id      INTEGER PRIMARY KEY AUTOINCREMENT,
    name    TEXT    NOT NULL,
    phone   TEXT    NOT NULL UNIQUE,
    group_name TEXT DEFAULT '朋友',
    created_at  DATETIME DEFAULT (datetime('now', 'localtime'))
);
```

它麻雀虽小：有主键自增、非空约束、唯一约束、默认值——日常开发中最常用的建表要素都在里面。

## 动手练习

1. 访问 SQLite 官网下载页，下载适合你系统的 amalgamation 包和命令行工具包，解压到固定目录；
2. 运行 `sqlite3 --version` 确认工具可用；
3. 用文本编辑器打开 `sqlite3.h`，搜索 `SQLITE_OK`，看看它的定义——你会发现它就是整数 `0`。这是所有 SQLite API 返回"成功"的统一约定。

<details>
<summary>参考答案（练习 3）</summary>

在 `sqlite3.h` 中可以找到：

```c
#define SQLITE_OK           0   /* Successful result */
/* beginning-of-error-codes */
#define SQLITE_ERROR        1   /* Generic error */
#define SQLITE_BUSY         5   /* The database file is locked */
#define SQLITE_ROW        100   /* sqlite3_step() has another row ready */
#define SQLITE_DONE       101   /* sqlite3_step() has finished executing */
```

记住两个关键点：

- 所有 API 成功时返回 `SQLITE_OK`（0），失败返回负数或正数错误码；
- `SQLITE_ROW`（100）和 `SQLITE_DONE`（101）**不是错误**，它们是 `sqlite3_step()` 用来报告"还有一行可取"和"执行完毕"的正常状态，第 5 章会反复用到。

</details>

## 本章小结

- SQLite 是**编译进程序的库**，不是独立服务；数据库就是一个文件；
- 单机、单进程（或低并发）场景首选 SQLite；多机高并发写入选服务器数据库；
- C/C++ 集成只需 `sqlite3.c` + `sqlite3.h` 两个文件，与普通源文件一起编译即可；
- `SQLITE_OK == 0`，`SQLITE_ROW == 100`，`SQLITE_DONE == 101`，这三个返回值贯穿全书。

## 下一章预告

材料备齐了，第 2 章我们用 `sqlite3` 命令行工具十分钟内建好 `contacts.db`——学会十几个点命令，你就有了一个随时可用的"数据库沙箱"，后面所有 SQL 都可以先在这里试错再写进代码。
