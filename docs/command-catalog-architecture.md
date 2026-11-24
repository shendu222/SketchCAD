# CommandCatalog 单一命令定义源：组织方式与调用关系

> 适用基线：本轮统一目录后的源码  
> 核心文件：`config/command_catalog.json`、`src/core/CommandCatalogConfig.h/.cpp`、`src/core/CommandCatalog.h/.cpp`  
> 维护界面：工具 → 命令目录维护（`src/ui/CommandCatalogEditor.h/.cpp`）  
> 本文说明 JSON 目录如何组织、谁读取它，以及新增/修改/删除命令时的维护边界。

---

## 1. 为什么统一

此前存在两类漂移：

1. `help <cmd>` 的部分详细条目来自 `CommandCatalog`，总表来自 `CommandInterpreter::commandNames()`，家族命令常出现“有实现、无详细帮助”。
2. AI 助手提示词内维护了一份 `kCore` 核心命令表和硬编码示例；目录更新后提示词仍可能停留在旧状态。

现在：

- `config/command_catalog.json` 收录全部 222 条内置命令（150 个精确命令 + 72 个前缀族命令）。
- JSON 的 `parameterSchema` 描述 `add(...)` 的 9 个形参：名称、类型、默认值、是否必填、说明、顺序。
- 程序启动时 `CommandCatalog::entries()` 读取 JSON，遍历 `commands[].parameters[]` 并自动完成全部 `add(...)` 注册。
- `CommandInterpreter::commandNames()` 直接返回 `CommandCatalog::names()`。
- `help` 总表与 `help <cmd>` 详情均读取目录；AI 助手全量注入目录元数据。
- Tab 补全、命令面板、COM 宿主命令列表也通过 `commandNames()` 间接读取目录。
- 维护只需修改 JSON（或运行“命令目录维护”界面），无需重新编译。

---

## 2. 目录组织

### 2.1 数据结构

每条命令是 `CommandInfo`：

| 字段 | 含义 |
|---|---|
| `name` | 小写命令名，与 `registerExact` / `registerPrefix` 实际可执行名一致。 |
| `zhName` | 中文显示名。 |
| `category` | 分组：绘图、修改、查询、标注、约束、选择、图层、块、视图、布局、追踪、样式、工具、脚本、帮助。 |
| `summary` | 一句话用途描述。 |
| `usage` | 完整命令行语法。 |
| `params` | 参数说明；空值由 `add()` 统一兜底。 |
| `example` | 可直接复制的命令样例；空值由 `add()` 用 `usage` 兜底。 |
| `related` | 相关命令名。 |
| `shortcut` | 默认快捷键（仅展示）。 |

### 2.2 JSON 文件结构

```json
{
  "schemaVersion": 1,
  "description": "命令注册目录说明",
  "parameterSchema": [
    { "order": 1, "name": "name", "type": "string", "default": "", "required": true, "description": "命令名" }
  ],
  "commands": [
    {
      "key": "line",
      "parameters": [
        { "order": 1, "name": "name", "value": "line" }
      ]
    }
  ]
}
```

- `parameterSchema`：描述 `add(...)` 的 9 个形参元数据，含顺序、名称、类型（`string` / `stringList`）、默认值、是否必填、说明。
- `commands[].key`：命令名，必须与 `name` 参数取值一致。
- `commands[].parameters[]`：该命令各形参的实际取值，按 `order` 与 `parameterSchema` 一一对应。

### 2.3 目录组织顺序

JSON 中命令按以下区块组织，便于 `help` 分组与 AI 阅读：

1. 基础绘图：`line` / `circle` / `arc` / `ellipse` / `rect` / `poly` / `spline` / `point` / `hatch` 等。
2. 直线与构造线族：`line_*`、`xline_*`。
3. 圆弧族：`arc_*`。
4. 矩形倒角/圆角与多边形族：`rect_*`、`poly_*`。
5. 样条族：`spline_*`、`freehand_line`。
6. 椭圆族：`ellipse_*`。
7. 折线编辑族：`polyline_*`。
8. 选择族。
9. 修改/标注/填充/符号/图框/宏/CUI 补录精确命令。
10. 查询、图层、块、视图、布局、追踪、工具、脚本、标准与帮助。

### 2.4 启动时注册流程

```text
MainWindow 构造 / CommandCatalog::entries()
  → CommandCatalogConfig::load(<exe>/config/command_catalog.json)
  → JSON 解析 + 结构校验（带 JSON 指针错误定位）
  → 遍历 commands[]
      → 遍历 parameters[]，按 schema 取 name/zhName/category/summary/usage/params/example/related/shortcut
      → add(...) 构建 CommandInfo
  → 缓存目录表，供 help / AI / 补全 / 面板共用
```

`add(...)` 仍保留原有兜底语义：`params` 为空用固定说明，`example` 为空用 `usage`，`usage` 为空用命令名；翻译仍由 `I18n::t` 处理。

---

## 3. 两个核心消费方

### 3.1 help 功能

```text
用户输入 help / help <cmd>
  → CommandInterpreter::execute()
  → HelpCommand::execute()
      ├─ help <cmd>: CommandCatalog::find(cmd) → 输出 zhName/summary/usage/params/example/shortcut/related
      └─ help 总表: CommandCatalog::names()（经 CommandInterpreter::commandNames()）
                     → 按 category 分组 → 输出命令清单 + 别名清单
```

总表命令名、计数、分组都由目录驱动；别名仍由 `AliasTable` 单列，因为别名不是命令定义，而是命令入口。

### 3.2 AI 提示词注入

```text
NlAssistant::buildSystemPrompt()
  → CommandCatalog::entries()
  → 按 category 分组
  → 拼入 "name（zhName）| 用途 | 用法 | 参数 | 示例"
  → 作为 System 消息传给模型
```

`getCommandHelp(name)` 工具同样调用 `CommandCatalog::find(name)`，与 `help <cmd>` 输出的是同一条目。  
`runSketchCommand` 的 schema 不再内嵌具体命令示例，只要求模型严格遵守 System 中注入的目录。

### 3.3 其他间接受益方

| 模块 | 调用路径 | 效果 |
|---|---|---|
| 命令行 Tab 补全 | `CommandLine → CommandInterpreter::completions → commandNames()` | 自动补全全部目录命令。 |
| 命令面板 | `CommandPalette → commandNames()` + `CommandCatalog::find()` | 标题/摘要/分类自动同步。 |
| COM 宿主命令列表 | `SketchCadHosting → commandNames()` | 自动化可见命令同步。 |
| 相近命令建议 | `help` 未知命令 → `commandNames()` 候选池 | 建议包含全部目录命令。 |

### 3.4 命令目录维护界面

入口：工具 → 命令目录维护...  
实现：`src/ui/CommandCatalogEditor.h/.cpp`

界面分为三区：

1. 命令清单：命令名、中文名、分类、用途；提供增加/删除/修改/查看。
2. 参数定义：全局 `parameterSchema` 的顺序、名称、类型、默认值、必填、说明；提供增加/删除/修改/查看。
3. 当前命令参数取值：选中命令的 9 项 add 实参；提供增加/删除/修改/查看。

校验与持久化：

- 参数名唯一、顺序正整数且唯一、类型仅 `string` / `stringList`。
- 说明不能为空；默认值与类型一致；必填参数取值不能为空。
- 命令名唯一；每条命令参数数量与 schema 一致；取值类型与 schema 一致。
- 点击“保存并生效”先执行 `CommandCatalogConfig::validate()`，通过后原子写入 JSON，再调用 `CommandCatalog::reload()`；`help` 与 AI 提示词立即生效。
- 配置缺失、读取失败、JSON 格式错误、结构错误均在启动和界面加载时显示包含绝对路径、行列/JSON 指针和原因的提示。

---

## 4. 维护规则

### 4.1 新增命令元数据

修改 `config/command_catalog.json` 的 `commands` 数组，或在维护界面点击“增加命令”。  
无需修改 `CommandCatalog.cpp`，无需重新编译；保存后 `help`、AI 提示词、补全、面板自动同步。

### 4.2 修改命令参数元数据

只改 JSON 中该命令对应 `parameters[].value`，或调整 `parameterSchema` 中的类型/默认值/必填/说明/顺序。  
维护界面会同步更新所有命令中同名参数的定义与取值。`help <cmd>`、AI 提示词、命令面板摘要随即同步。

### 4.3 删除命令元数据

从 JSON 的 `commands` 删除对应对象，或在维护界面点击“删除命令”。  
若命令仍可执行但无元数据，`help <cmd>` 会走“暂无详细帮助”回落；若仍有元数据但无执行注册，则命令面板可见但执行失败。

### 4.4 维护边界说明

`config/command_catalog.json` 是“命令定义/文档/参数元数据”的唯一来源，但不是命令执行注册表。  
`Commands.cpp` 的 `registerExact` / `registerPrefix` 仍决定命令能否被执行；这种分层让元数据层不依赖命令实现，并支持运行时重载。

---

## 5. 自测保障

新增测试覆盖：

- JSON 启动时可加载，`CommandCatalog::lastLoadError()` 为空。
- JSON 文档通过结构校验；每条命令 9 个 add 实参，名称唯一。
- 每条命令具有名称/用途/用法/参数/示例。
- 全部精确注册命令被目录收录；全部家族名单被目录收录。
- `CommandInterpreter::commandNames() == CommandCatalog::names()`。
- AI System 消息逐条包含目录命令名与用法。
- 必填参数为空时校验错误带 JSON 指针定位（如 `/commands[0]/...`）。
- 配置文件缺失时提示包含“配置缺失”与完整路径。
- `help rect_chamfer` 输出详细用法，证明家族命令已纳入目录。

静态复核结果：JSON 222 条、9 个参数定义、每条命令 9 个参数；命令名唯一；注册表与家族全集差集为空。
