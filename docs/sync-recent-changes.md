# 最近修改同步方案

> 实现脚本：`tools/sync_recent_changes.py`  
> 本方案把“刚才修改的内容”从当前工作区以差异更新方式同步到一个“最新版本”目录，并提供冲突处理与结果验证。

---

## 1. 源内容

“刚才修改的内容”定义为：

- 当前仓库工作区相对基线 `--base` 的 Git 差异，默认 `--base HEAD`。
- 包含：
  - 已跟踪文件的修改与删除；
  - 允许名单内的未跟踪新增文件。
- 自动排除：
  - `.git/`、`.codebuddy/`、`build*/`、`dist/`、`release/`；
  - 编译产物 `*.exe`、`*.dll`、`*.a`、`*.obj`、`*.o`、`*.pdb`、`*.qm`；
  - Python 缓存 `__pycache__/`、`*.pyc`。

默认包含路径（`--include` 可覆盖）：

```text
CMakeLists.txt, README*, RELEASE_NOTES.md, COPYING, LICENSE.txt,
src/**, tests/**, docs/**, config/**, sdk/**, tools/**,
packaging/**, resources/**, hatch/**, plotstyle_samples/**
```

本次实际识别到 32 个变更文件，包括 `config/command_catalog.json`、命令目录 JSON 模块、维护界面、AI 椭圆修复、DRY 重构、用户手册与自测更新等。

---

## 2. 目标“最新版本”

- 目标位置由 `--target` 指定。
- 本次验证使用：`build/sync-target/latest`。
- 若目标目录不存在且指定 `--init-from-head`，脚本会用 `git archive <base>` 将同一基线快照初始化到目标目录，作为“最新版本”起点。
- 目标也可以是另一个仓库/工作副本/发布目录，只要通过 `--target` 指向其根目录。

---

## 3. 同步方式

采用**三态差异更新**，不是整目录覆盖：

```text
source = 当前工作区文件内容
base   = git show <base>:<path>
target = 目标目录中对应文件内容
```

| 条件 | 动作 |
|---|---|
| source == target | `skip`，不写文件 |
| target == base，source 不同 | `update` / `add` / `delete`，差异写入 |
| target != base 且 source != base | `conflict`，按冲突策略处理 |

比较前统一把 CRLF/CR 归一化为 LF，避免 `git archive` 与工作区行尾差异造成伪冲突。

---

## 4. 冲突处理规则

`--on-conflict` 提供三种策略：

| 策略 | 行为 |
|---|---|
| `fail`（默认） | 不修改冲突文件，报告失败并返回退出码 2。适合严格同步。 |
| `backup` | 保留目标文件，把源版本写入同目录 `<文件名>.sync-incoming`，报告中记为 `conflict-backup`。 |
| `overwrite` | 以源内容覆盖目标内容，报告中记为 `overwrite-write` / `overwrite-delete`。 |

冲突判定只针对同一路径；目标独有文件默认不删除。

---

## 5. 验证机制

同步完成后自动执行：

1. 对每个 `add` / `update` / `delete` 结果做 SHA-256（行尾归一化后）一致性检查。
2. 对 `.json` 文件做语法解析。
3. 若提供 `--verify-command`，在目标目录执行该命令并要求退出码为 0。
4. 在目标根目录写出 `.sync-report.json`，包含：
   - 时间戳、源仓库、基线、目标路径；
   - 同步模式与冲突策略；
   - 每个文件的动作、结果、源/目标哈希；
   - 验证失败列表与总状态 `ok`。

退出码：

- `0`：同步与验证成功；
- `1`：参数/环境/读取错误；
- `2`：存在未解决冲突或验证失败。

---

## 6. 使用方法

```bash
# 试运行，仅输出计划
python tools/sync_recent_changes.py --target build/sync-target/latest --init-from-head --dry-run

# 正式同步到最新版本快照，并校验 JSON
python tools/sync_recent_changes.py \
  --target build/sync-target/latest \
  --init-from-head \
  --on-conflict fail \
  --verify-command "python -m json.tool config/command_catalog.json"

# 同步上一次提交
python tools/sync_recent_changes.py --base HEAD~1 --target ../sketchCAD-latest

# 冲突时保留目标并备份源版本
python tools/sync_recent_changes.py --target ../sketchCAD-latest --on-conflict backup
```

---

## 7. 本次验证结果

- 首次同步：`同步文件数 32，验证失败数 0`。
- 二次同步：32 个文件全部 `skip`，验证失败数 0，证明幂等。
- 冲突测试：人为修改目标端 `CMakeLists.txt` 后使用 `--on-conflict backup`：
  - 目标修改被保留；
  - 源版本写入 `CMakeLists.txt.sync-incoming`；
  - 报告准确记录 `conflict-backup`。
- `python -m json.tool config/command_catalog.json` 在目标目录执行通过。
