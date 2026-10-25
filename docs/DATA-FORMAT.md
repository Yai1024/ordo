# Ordo 数据文件规范

> 本文件是 `docs/OUTLINE.md` §9 的完整展开。字段有变化时，先改本文件与大纲，再改代码。

## 1. 文件清单

| 文件 | 阶段 | 作用 |
| --- | --- | --- |
| `data/tasks.csv` | M1 | 任务主数据，整表重写 |
| `data/config.ini` | M2 | 作息与装箱策略配置，`plan` 命令读取 |
| `data/log.csv` | M3 | 写操作事件日志，只追加（ADR-0004） |
| `data/samples/*.csv` | M4 | 边界测试数据集，只读 |

所有文件均为 **UTF-8（无 BOM）**、**LF 换行**。读取时容忍 BOM 与 CRLF。

---

## 2. `data/tasks.csv`

### 2.1 结构

首行固定为表头，之后每行一条任务：

```csv
id,title,course,importance,estimate,deadline,state,review_stage,kind,fixed_start,ease_factor,interval_days,last_review
1,"读书,第3章",高等数学,4,90,2026-10-30 23:59,todo,0,coursework,,2.50,0,
2,社团例会,课外,4,60,2026-10-28 20:00,todo,0,activity,2026-10-28 19:00,2.50,0,
3,线性代数复习,线性代数,5,45,2026-11-10 23:59,todo,2,review,,2.36,6,2026-10-23 20:00
```

字段顺序固定，**新增字段只能追加到行尾**（保证旧文件可读）。
读取时同时接受 8 列（M1）、10 列（M2）与 13 列（M3）表头：

- 8 列：`kind=other`、`fixed_start` 空、`ease_factor=2.5`、`interval_days=0`、`last_review` 空。
- 10 列：后三列使用同样的默认值。

保存时统一写 13 列表头，旧文件在第一次写入时自动迁移。

### 2.2 字段定义

| # | 字段 | 类型 | 约束 | 说明 |
| --- | --- | --- | --- | --- |
| 1 | `id` | uint64 | 唯一，> 0 | 新增任务时由程序分配（当前最大 id + 1） |
| 2 | `title` | string | 非空，不含换行 | 含逗号或双引号时按 CSV 规则加引号 |
| 3 | `course` | string | 可空 | 课程名，仅用于展示与分组 |
| 4 | `importance` | int | 1–5 | 重要度，越大越优先 |
| 5 | `estimate` | int | ≥ 0，单位分钟 | 预估耗时 |
| 6 | `deadline` | datetime | `YYYY-MM-DD HH:MM` | 本地时间；只给日期时按当天 23:59 |
| 7 | `state` | enum | `todo` / `doing` / `done` / `dropped` | 任务状态 |
| 8 | `review_stage` | int | ≥ 0 | SM-2 重复次数，0 表示未进入复习（M3 使用） |
| 9 | `kind` | enum | `coursework` / `selfstudy` / `activity` / `review` / `other` | 任务类型，决定打分系数与装箱方式；旧行留空按 `other` |
| 10 | `fixed_start` | datetime | 仅 `activity` 必填，其余必须为空 | 活动的固定开始时刻；活动要求 `estimate > 0` 且 `deadline ≥ fixed_start + estimate` |
| 11 | `ease_factor` | real | `[1.3, 5.0]`，两位小数 | SM-2 难度系数；旧行默认 `2.50` |
| 12 | `interval_days` | int | `0–365` | 当前复习间隔（天）；0 表示尚未进入复习 |
| 13 | `last_review` | datetime | 可空 | 上次复习时刻；空表示从未复习 |

任务类型与编排规则见 `docs/adr/0003-planning.md`；复习字段与统计口径见
`docs/adr/0004-tracking.md`。

### 2.3 CSV 转义规则

- 字段含 `,`、`"`、`\r`、`\n` 时必须用双引号包住。
- 字段内的 `"` 写成 `""`。
- 例如标题 `他说"好"` 写作 `"他说""好"""`。
- 空字段合法（连续逗号），例如 `a,,c`。

---

## 3. `data/config.ini`

```ini
[schedule]
weekday_start = 18:30
weekday_end   = 23:00
weekend_start = 09:00
weekend_end   = 22:00
break_minutes = 10

[policy]
default_estimate = 60
allow_split      = true
min_block        = 15
```

- M2 起 `plan today` / `plan week` 读取本文件：`weekday_*` / `weekend_*` 推导空闲时段，
  `policy.*` 决定装箱策略。
- 文件不存在时使用内置默认值（与上表一致）；键值非法时报错并带行号，不静默降级。
- 格式为 INI：`key = value`，`#` 或 `;` 开头为注释；未知键忽略，便于以后追加配置。

---

## 4. `data/log.csv`

只记录写操作的事件日志（ADR-0004），表头固定为 5 列：

```csv
timestamp,command,task_id,minutes,note
2026-10-25 20:15:03,task.done,5,45,交税收作业
2026-10-25 20:20:41,task.review,3,30,quality=4
```

| # | 字段 | 类型 | 约束 | 说明 |
| --- | --- | --- | --- | --- |
| 1 | `timestamp` | datetime | `YYYY-MM-DD HH:MM:SS` | 动作时刻（本地时间），带秒 |
| 2 | `command` | enum | `task.add` / `task.edit` / `task.done` / `task.remove` / `task.review` | 动作名，CLI 与 GUI 共用 |
| 3 | `task_id` | uint64 | 可空，> 0 | 关联任务；任务删除后仍保留 |
| 4 | `minutes` | int | 可空，≥ 0 | 实际投入；只有显式给出 `--spent` 时才写 |
| 5 | `note` | string | 可空 | 备注；写操作默认写任务标题，复习写 `quality=N` |

- 只追加，不重写；文件不存在或为空时，第一次写入先写表头。
- 读取时容忍 BOM、CRLF 与空行；字段数不为 5、引号未闭合、时间或数字格式错误 →
  `Corrupted` / `InvalidFormat`，整表拒绝加载。
- 写入失败**不阻断**任务操作；日志是旁路审计（ADR-0004 决策 6）。

### 4.1 周统计口径

`stats week` 与 GUI 统计面板都按以下规则计算最近 7 个自然日：

- 窗口：今天 00:00 往前推 6 天，到调用时刻。
- 完成数：窗口内出现 `task.done` 的不同任务数（重复标记只算一次）。
- 应完成数：窗口内完成的任务与"当前未完成且截止时间在窗口内"的任务的并集。
- 完成率：完成数 / 应完成数；分母为 0 时显示 `--`。
- 投入分布：窗口内日志 `minutes` 之和，按任务当前类型分组；任务已删除归入"其他"。
- 启用日志之前的历史不会被估算补入。

---

## 5. 读取与容错

| 情况 | 行为 | 错误码 |
| --- | --- | --- |
| 文件不存在 | 视为空表，首次保存时自动创建 | 无 |
| 空文件 / 只有空行 | 视为空表 | 无 |
| 表头不匹配 | 整表拒绝加载 | `Corrupted` |
| 字段数不是 8 / 10 / 13（对应三代表头） | 整表拒绝加载 | `Corrupted` |
| 引号未闭合 | 整表拒绝加载 | `Corrupted` |
| 数值字段不是整数 | 整表拒绝加载 | `InvalidFormat` |
| 日期格式错误 | 整表拒绝加载 | `InvalidFormat` |
| 状态值非法 | 整表拒绝加载 | `InvalidFormat` |
| 字段值越界（如 importance=9） | 整表拒绝加载 | `InvalidValue` |
| id 重复 | 整表拒绝加载 | `Duplicate` |

**原则：宁可整表报错，也不静默丢行。** 报错信息带行号，用户能直接定位。

---

## 6. 写入规则

1. `tasks.csv` 先写 `tasks.csv.tmp`，全部成功后再原子替换；任何一步失败都删除临时文件。
2. 写入前逐条调用 `Task::validate()`，非法记录不落盘。
3. 保存采用整表重写（数据量为百条级，无需增量更新）。
4. `log.csv` 采用追加写，不做临时文件替换；日志失败不影响任务保存。

---

## 7. 命令与字段的对应

| 命令 | 影响的字段 |
| --- | --- |
| `task add` | `id` 自动分配；`title/course/importance/estimate/deadline/kind/fixed_start` 来自参数；`state=todo`、`review_stage=0`、`ease_factor=2.50`、`interval_days=0`、`last_review` 空 |
| `task edit` | 只改显式给出的字段，其余保持原值（含复习状态） |
| `task done [--spent 分钟]` | `state` → `done`；日志写 `task.done`，`minutes` 取 `--spent` |
| `task remove` | 删除整行；日志写 `task.remove` |
| `task review <id> --quality 0..5 [--spent 分钟]` | 按 SM-2 更新 `review_stage/ease_factor/interval_days/last_review`，`state` → `todo`；日志写 `task.review` |
| `task review`（不带 id） | 只读，列出到期复习任务 |
| `task list --rank score` | 不写文件，按加权分数只读排序 |
| `task list --rank edf` | 不写文件，按截止时间只读排序 |
| `stats week` | 不写文件，只读聚合 `log.csv` 与 `tasks.csv` |
