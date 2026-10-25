# GUI 接入契约（`ui::AppModel`）

> 本文件写给"将来要换一个图形外壳"的人。当前外壳是 Win32（`src/gui/`），
> 但界面逻辑全部在 `include/ordo/ui/app_model.h` 之后，外壳可以整体替换。

## 1. 分层位置

```
gui/（任何外壳）──▶ ui::AppModel ──▶ app::TaskService ──▶ { algo/, io/ }
```

- `gui/`：窗口、控件、键盘、绘制。**不含业务规则**。
- `ui::AppModel`：表单解析、校验调用、展示行映射、查询/统计入口。
  **不出现任何 Win32 类型**（`HWND`、`wchar_t`、消息）。
- `app::TaskService`：唯一实现业务规则的地方。

外部 GUI（Qt / 网页 / ImGui）只需要实现"怎么显示、怎么收集输入"，
其余直接调用 `AppModel`，不得绕过它去读写 CSV。

## 2. 构造

```cpp
#include "ordo/io/file_repository.h"
#include "ordo/ui/app_model.h"

ordo::CsvTaskRepository repo("data/tasks.csv");
auto config = ordo::load_config("data/config.ini");
ordo::AppModel model(repo, [] { return ordo::Clock::now(); }, config.value());
```

- `repo` 必须是 `ITaskRepository&`，生命周期长于 `AppModel`。
- 时钟以函数注入：测试时返回固定时间，正式运行时用 `Clock::now()`。
- `ScheduleConfig` 由装配点读取后注入；缺省构造使用内置默认值。
- `AppModel` 自身不保存界面状态（选中行、滚动位置由外壳自己管）。

## 3. 数据类型

### `ListQuery` — 查询条件

| 字段 | 含义 |
| --- | --- |
| `state` | 指定某个状态；为空表示不按状态过滤 |
| `open_only` | `state` 为空时，是否只保留 `todo` / `doing` |
| `rank` | `RankMode::None / Score / Edf` |

### `TaskRow` — 列表一行

`id`、`title`、`course`、`kind_label`（中文类型名）、`importance`、`estimate`、
`deadline_text`（`YYYY-MM-DD HH:MM`）、`state`。
字符串已经是 UTF-8，外壳负责转成自己的显示编码。

### `TaskForm` — 新增/编辑表单

`title`、`course`、`kind`、`importance`、`estimate`、`deadline`、`start` 都是**字符串**，
按用户在控件里的原始输入传入；解析与校验由 `AppModel` 完成。
`kind` 取值 `coursework/selfstudy/activity/review/other`；`start` 只在活动时使用
（`YYYY-MM-DD HH:MM`），此时 `deadline` 可以留空，由 `AppModel` 按
`start + estimate` 自动补全。`state` 只在编辑时使用。

## 4. 方法

| 方法 | 返回 | 说明 |
| --- | --- | --- |
| `list(query)` | `Result<std::vector<TaskRow>>` | 列表查询与排序 |
| `stats()` | `Result<Stats>` | 总数、各状态计数、超期数、预估总时长 |
| `form_for(id)` | `Result<TaskForm>` | 编辑对话框的初始值 |
| `create(form)` | `Result<TaskId>` | 新增，成功返回分配到的 id |
| `update(id, form)` | `Status` | 修改，保留 `review_stage` / `last_review` |
| `remove(id)` | `Status` | 删除 |
| `set_state(id, state)` | `Status` | 改状态（标记完成、进行中等） |
| `plan_today(rank)` | `Result<PlanView>` | 今日计划：时间块 + 告警 + 时长汇总 |
| `plan_week(rank)` | `Result<PlanView>` | 滚动 7 天计划 |

失败一律通过 `Status { code, message }` 返回，`message` 是可直接显示的中文。

## 5. 一个外壳需要实现什么

以"标记完成"为例：

1. 用户选中一行，外壳记下 `TaskRow.id`；
2. 调 `model.set_state(id, TaskState::Done)`；
3. `Status` 失败 → 显示 `status.message`；
4. 成功 → 重新 `model.list(current_query())` 并刷新列表与 `model.stats()`。

新增/编辑：

1. 调 `model.form_for(id)` 取初始值（新增时用默认值）；
2. 用户提交后组装 `TaskForm`；
3. 调 `create` / `update`；
4. 失败时把 `message` 显示在表单里，**不要关闭对话框**。

## 6. 编码与线程

- 契约层字符串统一 **UTF-8**。Win32 外壳用 `to_wide/to_utf8` 转换；
  Qt 可直接 `QString::fromUtf8`；网页可直接输出。
- 当前实现是单线程。若外壳在后台线程调用 `AppModel`，需要自己加锁，
  `ITaskRepository` 与 `CsvTaskRepository` 都不是线程安全的。

## 7. 计划视图（M2 已定稿）

`AppModel` 提供两个方法，返回已经格式化好的展示数据：

```cpp
struct PlanRow {
    std::string time_text;    // "18:30 - 19:30"
    std::string title;        // 任务标题
    std::string kind_label;   // 课程作业 / 自主学习 / 活动 / 复习考试 / 其他
};

struct PlanView {
    std::vector<PlanRow> rows;      // 按开始时间升序
    std::vector<std::string> warnings;  // 已带任务编号与中文原因
    Minutes total_free{0};
    Minutes total_assigned{0};
};

Result<PlanView> plan_today(RankMode rank = RankMode::Score) const;
Result<PlanView> plan_week(RankMode rank = RankMode::Score) const;
```

外壳只需要：

1. 调 `plan_today()` / `plan_week()`；
2. 失败时显示 `status.message`；
3. 成功时渲染 `rows`、`warnings` 与两个时长数字。

计划不持久化，每次调用都按当前任务与配置重新计算。
`AppModel` 的现有方法签名保持稳定；新增方法向后兼容。

## 8. 替换 Win32 外壳

删除 `src/gui/` 与 Makefile 里的 GUI 目标不会影响 `ordo.exe`、`ordo_test.exe`。
新外壳只需：

1. 在 `main` 里构造 `ITaskRepository` 与 `AppModel`；
2. 实现列表、表单、统计面板的显示；
3. 所有动作调用第 4 节的方法。

不需要改 `ui/`、`app/`、`algo/`、`io/` 中任何一行。
