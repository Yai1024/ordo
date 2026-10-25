# ADR-0003 · 任务类型与 M2 编排

| 项 | 值 |
| --- | --- |
| 状态 | 已采纳（Accepted） |
| 日期 | 2026-10-25 |
| 决策人 | Jerry |
| 关联 | `docs/OUTLINE.md` §5.2 §6 §7.4 §7.5 §9 §10、`docs/DATA-FORMAT.md`、`docs/GUI-INTERFACE.md` |

本记录定义 M2（编排）的模型与算法语义。起因是用户要求：任务不能只是一个抽象分数，
而要能表达**课程作业、自主学习、活动、复习考试**这些具体身份，并让编排按身份区别对待。

---

## 决策 1 · 引入 `TaskKind`，活动带固定开始时间

### 背景

M1 的 `Task` 只有 `importance`（1–5）与 `deadline`。这足以排序，但无法表达：
课程作业必须交、活动时间不能挪、自学可以往后放。仅靠重要度数字，用户要自己把
"活动"翻译成高分，而且装箱算法无法知道"这块时间不能动"。

### 决定

在 `core/task.h` 增加：

```cpp
enum class TaskKind { CourseWork, SelfStudy, Activity, Review, Other };

struct Task {
    // ... 原有字段 ...
    TaskKind kind{TaskKind::Other};
    std::optional<TimePoint> fixed_start;  // 仅 Activity 使用
};
```

| 类型 | CSV 文本 | 语义 |
| --- | --- | --- |
| 课程作业 | `coursework` | 有硬截止，可拆分，权重最高 |
| 自主学习 | `selfstudy` | 弹性任务，可拆分，权重较低 |
| 活动 / 固定安排 | `activity` | **开始时刻固定**，整块占位，不可拆分 |
| 复习 / 考试 | `review` | 截止驱动，可拆分，权重较高 |
| 其他 | `other` | 旧数据默认值，行为同自主学习 |

### 不变量

- `Activity`：`fixed_start` 必须存在；`estimate > 0`；`deadline >= fixed_start + estimate`。
- 非 `Activity`：`fixed_start` 必须为空。
- 其余校验沿用 M1（标题非空、重要度 1–5、预估非负、截止非空）。

### CSV 迁移

`tasks.csv` 表头在行尾追加 `kind,fixed_start`：

```csv
id,title,course,importance,estimate,deadline,state,review_stage,kind,fixed_start
```

- 读取时**同时接受**旧 8 列表头与新 10 列表头；旧文件缺少的两列按 `other` / 空处理。
- 保存时统一写新表头，旧文件在第一次写入时自动迁移。
- 字段仍只追加到行尾，旧版本程序读新文件会因为列数不符而拒绝，符合"宁可报错"原则。

### 后果

- **正面**：编排能区分"必须完成的作业"和"可挪动的自学"；活动成为真正的时间约束。
- **负面**：`Task`、CSV、CLI 参数、GUI 表单、测试都要同步扩展，M2 工作量增加。
- **约束**：`Activity` 的固定时间只来自任务本身，不引入 `busy.csv`；
  用户的其他固定占用留到 M3。

---

## 决策 2 · 类型参与打分与装箱

### 打分

`score_weighted` 在原有三项（重要度、紧急度、短任务）之外，乘一个类型系数：

| 类型 | 系数 |
| --- | --- |
| 课程作业 | 1.15 |
| 复习 / 考试 | 1.10 |
| 其他 | 1.00 |
| 自主学习 | 0.90 |
| 活动 | 不参与排序（固定占位） |

系数只调整相对顺序，不改变"超期优先"和"并列时早截止优先"的既有规则。

### 装箱

- 活动：直接生成 `[fixed_start, fixed_start + estimate)` 时间块，并从空闲时段中扣除；
  同一时段的其他任务不会排进来。活动开始时间已过则生成 `Warning` 并跳过。
- 非活动：按排序结果贪心填充空闲时段。
  - `allow_split = true` 时允许跨块拆分；每个块之后保留 `gap` 分钟。
  - 单块不短于 `min_block`；剩余量低于 `min_block` 时不再单独成块，生成 `Warning`。
  - `allow_split = false` 时只接受能一次装下整个任务的连续时段。
  - 块结束不得晚于 `deadline`；已超期任务忽略截止约束、排最早可用时段，并给告警。

---

## 决策 3 · 今日与本周的窗口语义

| 计划 | 窗口 | 参与任务 |
| --- | --- | --- |
| `plan today` | `max(now, 今日作息开始)` → 今日作息结束 | **所有未完成任务**，按优先级填满空闲时间 |
| `plan week` | `now` → 第 7 个自然日的作息结束（滚动 7 天） | 只含**截止落在窗口内**的未完成任务（含已超期） |

- 每天的空闲时段由 `config.ini` 的 `weekday_*` / `weekend_*` 决定；
  周末按本地时间的周六、周日判断。
- 今日计划面向"今天还能做什么"，因此不按截止过滤；本周计划面向"本周要交的活"，
  避免把下个月的作业排进本周。
- 活动只要落在窗口内就展示，即使它不在作息窗口内（例如白天的课）；
  它不会减少作息窗口之外的空闲时间。

### 告警

`Plan.warnings` 承载所有"排不上"或"需要注意"的情况，至少包括：

- 可用时间不足（给出还差多少分钟）
- 剩余量低于 `min_block`
- 已超期但已排入
- 活动时间已过
- 时间块重叠（冲突检测）

排不上不是错误，是正常业务结果；`PlanService` 仍返回 `Result<Plan>` 的成功值。

---

## 决策 4 · 配置读取放在 `io/config.h`

### 背景

`PlanService` 需要作息窗口与装箱策略。OUTLINE §5.2 原先没有登记配置读取文件，
而 `algo/` 不允许做 I/O，`app/` 不应自己解析 INI。

### 决定

新增 `include/ordo/io/config.h` + `src/io/config.cpp`：

```cpp
struct ScheduleConfig {
    Minutes weekday_start{18 * 60 + 30};
    Minutes weekday_end{23 * 60};
    Minutes weekend_start{9 * 60};
    Minutes weekend_end{22 * 60};
    Minutes break_minutes{10};
    Minutes default_estimate{60};
    bool    allow_split{true};
    Minutes min_block{15};
};

Result<ScheduleConfig> load_config(const std::filesystem::path& path);
```

- 文件不存在 → 返回默认值（与 `tasks.csv` 的"首次运行视为空表"一致）。
- 键值非法 → `Result` 失败，带行号；不静默使用默认值。
- 未知键忽略，便于以后追加配置而不破坏旧文件。
- `main.cpp` / `main_gui.cpp` 作为装配点读取配置，放入 `AppContext` 或 `AppModel`；
  `algo/` 与 `ui/` 不读文件。

---

## 决策 5 · 文件登记

本 ADR 对应在 `docs/OUTLINE.md` §5.2 登记的新文件：

```
core/slot.h/.cpp
core/schedule.h/.cpp
algo/packer.h/.cpp
algo/conflict.h/.cpp
io/config.h/.cpp
app/plan_service.h/.cpp
cli/commands/plan_cmd.h/.cpp
tests/test_packer.cpp
tests/test_config.cpp
tests/test_plan_service.cpp
docs/adr/0003-planning.md
```

---

## 变更记录

| 日期 | 变更 |
| --- | --- |
| 2026-10-25 | 初版：任务类型、活动固定时间、打分系数、装箱与窗口语义、配置读取 |
