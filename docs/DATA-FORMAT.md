# Ordo 数据文件规范

> 本文件是 `docs/OUTLINE.md` §9 的完整展开。字段有变化时，先改本文件与大纲，再改代码。

## 1. 文件清单

| 文件 | 阶段 | 作用 |
| --- | --- | --- |
| `data/tasks.csv` | M1 | 任务主数据，程序唯一的写入目标 |
| `data/config.ini` | M1（M2 起读取） | 作息与策略配置；当前版本保留配置位 |
| `data/log.csv` | M3 | 每次运行的执行日志，追加写 |
| `data/samples/*.csv` | M4 | 边界测试数据集，只读 |

所有文件均为 **UTF-8（无 BOM）**、**LF 换行**。读取时容忍 BOM 与 CRLF。

---

## 2. `data/tasks.csv`

### 2.1 结构

首行固定为表头，之后每行一条任务：

```csv
id,title,course,importance,estimate,deadline,state,review_stage
1,"读书,第3章",高等数学,4,90,2026-10-30 23:59,todo,0
```

字段顺序固定，**新增字段只能追加到行尾**（保证旧文件可读）。

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
| 8 | `review_stage` | int | ≥ 0 | SM-2 复习阶段，0 表示未进入复习（M3 使用） |

`last_review` 只存在于内存中的 `Task` 结构，**不写入 CSV**。

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

- 当前版本（M1）不读取本文件，算法使用内置默认值（每日可投入 480 分钟）。
- M2 编排功能会读取 `weekday_*` / `weekend_*` 推导空闲时段，读取 `policy.*` 决定装箱策略。
- 格式为 INI：`key = value`，`#` 或 `;` 开头为注释。

---

## 4. 读取与容错

| 情况 | 行为 | 错误码 |
| --- | --- | --- |
| 文件不存在 | 视为空表，首次保存时自动创建 | 无 |
| 空文件 / 只有空行 | 视为空表 | 无 |
| 表头不匹配 | 整表拒绝加载 | `Corrupted` |
| 字段数不是 8 | 整表拒绝加载 | `Corrupted` |
| 引号未闭合 | 整表拒绝加载 | `Corrupted` |
| 数值字段不是整数 | 整表拒绝加载 | `InvalidFormat` |
| 日期格式错误 | 整表拒绝加载 | `InvalidFormat` |
| 状态值非法 | 整表拒绝加载 | `InvalidFormat` |
| 字段值越界（如 importance=9） | 整表拒绝加载 | `InvalidValue` |
| id 重复 | 整表拒绝加载 | `Duplicate` |

**原则：宁可整表报错，也不静默丢行。** 报错信息带行号，用户能直接定位。

---

## 5. 写入规则

1. 先写 `tasks.csv.tmp`，全部成功后再原子替换 `tasks.csv`。
2. 任何一步失败都删除临时文件，原数据保持完整。
3. 写入前逐条调用 `Task::validate()`，非法记录不落盘。
4. 保存采用整表重写（数据量为百条级，无需增量更新）。

---

## 6. 命令与字段的对应

| 命令 | 影响的字段 |
| --- | --- |
| `task add` | `id` 自动分配；`title/course/importance/estimate/deadline` 来自参数；`state=todo`、`review_stage=0` |
| `task edit` | 只改显式给出的字段，其余保持原值 |
| `task done` | `state` → `done` |
| `task remove` | 删除整行 |
| `task list --rank score` | 不写文件，按加权分数只读排序 |
| `task list --rank edf` | 不写文件，按截止时间只读排序 |
