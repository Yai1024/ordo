# ADR-0004 · M3 长期能力：事件日志、周统计与 SM-2

| 项 | 值 |
| --- | --- |
| 状态 | 已采纳（Accepted） |
| 日期 | 2026-10-25 |
| 决策人 | Jerry |
| 关联 | `docs/OUTLINE.md` §5.2 §6 §7.5 §9.3 §10、`docs/DATA-FORMAT.md`、`docs/GUI-INTERFACE.md` |

本记录定义 M3 的语义。三件事互相咬合：**日志**记录发生了什么，**统计**从日志回答
"这周做得怎么样"，**SM-2** 决定复习任务下一次什么时候出现。M2 之前不写 SM-2 的纪律
到期解除。

---

## 决策 1 · 只记录写操作的事件日志

### 背景

OUTLINE §2.1 说"每天运行一次，`log.csv` 就多一行"，但字面执行会把 `task list`、
`plan today` 这类只读命令也写进日志。日志一旦被查询噪音填满，周统计就无法回答
"这周到底完成了什么、投入了多少"。

### 决定

新增 `data/log.csv`，表头：

```csv
timestamp,command,task_id,minutes,note
```

| 字段 | 类型 | 约束 | 说明 |
| --- | --- | --- | --- |
| `timestamp` | datetime | `YYYY-MM-DD HH:MM:SS` | 动作发生的本地时刻；带秒，保证同分钟内多次操作可排序 |
| `command` | string | 见下表 | 动作名，CLI 与 GUI 使用同一套名字 |
| `task_id` | uint64 | 可空 | 关联任务；删除任务后仍保留 |
| `minutes` | int | 可空，≥ 0 | 实际投入；只有显式给出时才写 |
| `note` | string | 可空 | 备注；写操作默认写任务标题 |

只记录**改变数据或记录投入**的动作：

| `command` | 触发点 | `minutes` |
| --- | --- | --- |
| `task.add` | 新增任务 | 空 |
| `task.edit` | 修改任务 / 改状态（非完成） | 空 |
| `task.done` | 标记完成（`task done --spent` 可带投入） | 可空 |
| `task.remove` | 删除任务 | 空 |
| `task.review` | 复习评分（`task review --spent` 可带投入） | 可空 |

- `task list` / `task stats` / `plan today` / `plan week` / `stats week` **不写日志**。
- GUI 的添加、编辑、完成、删除、复习通过同一套服务层落日志，命令名与 CLI 完全一致，
  因此日志不区分入口。
- 日志是**追加写**：不重写历史，不做临时文件替换。启动时若文件不存在则先写表头。
- 日志写入失败**不阻断**业务操作：任务已经成功保存后，日志是旁路审计；`append`
  返回 `Status`，服务层记录但不上抛。理由见决策 6。

### 后果

- **正面**：日志可直接聚合；读命令跑一万次也不会污染统计。
- **负面**：日志不是"运行流水账"，排查"谁在什么时候启动过程序"不能靠它。
- **约束**：新增写命令时必须同时补 `command` 名称与文档，否则统计会漏。

---

## 决策 2 · `tasks.csv` 追加三列，保存时迁移

### 背景

真正的 SM-2 需要保留 `ease_factor`（难度系数）、`interval_days`（当前间隔）与
`last_review`（上次复习时刻）。M1 只有 `review_stage`，DATA-FORMAT 还明确写着
`last_review` 不写盘——那会让重启后的复习计划全部丢失。

### 决定

`tasks.csv` 表头在行尾追加三列，共 13 列：

```csv
id,title,course,importance,estimate,deadline,state,review_stage,kind,fixed_start,ease_factor,interval_days,last_review
```

| 新字段 | 类型 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `ease_factor` | real | `2.5` | SM-2 难度系数，范围 `[1.3, 5.0]`；两位小数写盘 |
| `interval_days` | int | `0` | 当前复习间隔（天）；0 表示尚未进入复习 |
| `last_review` | datetime | 空 | 上次复习时刻；空表示从未复习 |

读取时**同时接受** 8 列（M1）、10 列（M2）与 13 列（M3）表头：

- 8 列：`kind=other`、`fixed_start` 空、`ease_factor=2.5`、`interval_days=0`、
  `last_review` 空。
- 10 列：后三列使用默认值。
- 保存时统一写 13 列表头，旧文件在第一次写入时自动迁移。

`last_review` 由此**进入持久化**；DATA-FORMAT 中"只存在于内存"的旧说明作废。

---

## 决策 3 · SM-2 语义

### 评分

`task review <id> --quality 0..5 [--spent 分钟]` 与 GUI 的"复习"按钮调用同一个服务方法。
质量含义沿用 SM-2：0 完全忘记，3 勉强想起，5 毫不费力。

### 更新规则

设当前 `(stage, ease, interval)`，评分 `q`：

1. 更新难度系数：
   `ease' = clamp(ease + 0.1 - (5-q) * (0.08 + (5-q) * 0.02), 1.3, 5.0)`
2. 若 `q < 3`：`stage' = 0`，`interval' = 1`（明天再来）。
3. 若 `q >= 3`：
   - `stage == 0` → `interval' = 1`
   - `stage == 1` → `interval' = 6`
   - 其余 → `interval' = round(interval * ease)`（用更新前的 ease）
   - `stage' = stage + 1`
4. `interval'` 上限 `365` 天，避免长期使用后溢出或出现荒谬的间隔。
5. `last_review' = now`，`state' = Todo`：一次复习完成后任务重新打开，等待下一个间隔。

只允许对 `kind == review` 的任务评分；其他类型返回 `InvalidValue`，不静默接受。

### 到期判断

```
从未复习（last_review 为空）           → 到期
now >= last_review + interval_days 天  → 到期
否则                                   → 未到期
```

- 新建的复习任务立即到期，用户可先做一次"摸底"评分。
- `task list` 增加复习状态后缀（`到期` / `N 天后` / `未复习`）；
  `task review` 不带 id 时列出全部到期任务。
- 到期只是展示与筛选条件，**不**自动写回任务，也**不**改变状态。

### 为什么不是固定 1/3/7/15 阶梯

固定阶梯实现更短，但"阶段"无法表达同一个任务对不同人难度不同。SM-2 的 ease 让
"总是记不住"的任务间隔变短，"很熟"的任务间隔拉长；数据仍然只是三个数值。

---

## 决策 4 · 周统计只看日志

### 窗口

`stats week` 的窗口是**最近 7 个自然日**：从今天 00:00 往前推 6 天，到调用时刻为止。
例如 2026-10-25 20:00 调用，窗口是 `2026-10-19 00:00` 到 `2026-10-25 20:00`。

### 指标

- **完成数**：窗口内出现 `task.done` 的不同任务数（同一任务重复标记只算一次）。
- **应完成数**：下面两类任务 id 的并集大小：
  1. 窗口内完成的（上一项）；
  2. 当前仍未完成、且截止时间落在窗口内的任务。

  并集避免"完成了且截止也在窗口内"被算两次。
- **完成率**：`完成数 / 应完成数`；分母为 0 时显示 `--`。
- **投入分布**：窗口内所有日志行的 `minutes` 之和，按任务**当前类型**分组；
  任务已被删除时归入"其他"。没有 `minutes` 的日志不计入投入。

**已知局限**：统计完全基于日志，启用日志之前完成的历史任务不会出现在本周数据里。
这是"从今天开始记录"的正常代价，在 `docs` 与命令输出中注明，不做估算兜底。

---

## 决策 5 · 文件登记

在 `docs/OUTLINE.md` §5.2 登记的新文件：

```
io/event_log.h/.cpp
algo/sm2.h/.cpp
cli/commands/stats_cmd.h/.cpp
tests/test_sm2.cpp
tests/test_event_log.cpp
```

`TaskService` 扩展 `set_state` / `grade_review` / `due_reviews` / `weekly_stats`，
不新增服务文件；复习评分界面复用 `gui/main_window.cpp` 内的模态窗口，不新增 GUI 文件。
M3 新增源文件约 8 个，§5.3 的预算同步从"约 4"改为"约 8"。

---

## 决策 6 · 日志失败不阻断业务

日志是**旁路审计**，不是业务数据。任务的增删改查已经成功落盘后，如果 `log.csv`
因磁盘只读、被占用等原因写不进去，正确的结果仍然是"任务操作成功"，而不是回滚任务。

实现上：`TaskService` 在业务动作成功后调用 `IEventLog::append`，忽略返回的 `Status`。
这不是"静默吞错"——`CsvEventLog` 自身会把错误写进 `Status`，只是没有上抛；
`AppContext` 不打印它。若以后需要可见的告警，应在 `Stats` 中加"日志缺失"字段，
而不是让用户的任务操作失败。

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-25 | 初版：写操作日志、13 列迁移、SM-2 规则、周统计口径 |
