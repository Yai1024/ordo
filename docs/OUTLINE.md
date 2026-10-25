# Ordo 项目大纲与实施指导

> **本文件是工程的唯一指导文件。** 任何实现若与本文件不一致，必须先修订本文件并记入 ADR，
> 再改代码。禁止"代码先行、文档追认"。
>
> - 文档版本：`0.2.0-draft`
> - 生效日期：2026-10-25
> - 适用仓库：`project_basic_cpp`（GitHub `Yai1024/ordo`，私有，main 分支）
> - 对应课程：《高级语言程序设计》综合实验大作业 · **选题 01 学习任务安排系统**
> - 维护人：Jerry

---

## 目录

- [0. 文档定位与阅读顺序](#0-文档定位与阅读顺序)
- [1. 项目标识](#1-项目标识)
- [2. 目标与范围](#2-目标与范围)
- [3. 术语表](#3-术语表)
- [4. 分层架构](#4-分层架构)
- [5. 文件预期架构](#5-文件预期架构)
- [6. 领域模型](#6-领域模型)
- [7. 接口与调用类型](#7-接口与调用类型)
- [8. 实现方式](#8-实现方式)
- [9. 数据文件规范](#9-数据文件规范)
- [10. 里程碑与验收对照](#10-里程碑与验收对照)
- [11. 测试策略](#11-测试策略)
- [12. 变更流程](#12-变更流程)
- [13. 索引](#13-索引)

---

## 0. 文档定位与阅读顺序

| 你想做什么 | 先读 |
| --- | --- |
| 了解项目要交付什么 | §1 §2 §10 |
| 看懂代码目录 | §4 §5 |
| 动手写第一个类 | §6 §7.6 |
| 写某个具体模块 | 直接跳到 §5 对应文件条目 |
| 答辩/写报告 | §10 §11 |
| 想大改设计 | §12 |

**本文件不写实现细节**（循环怎么写、边界怎么判），那些写在各模块头文件的注释里。
本文件只回答三件事：**有哪些文件、它们怎么分层、它们之间怎么互相调用**。

---

## 1. 项目标识

| 项 | 值 |
| --- | --- |
| 项目代号 | **Ordo**（拉丁语 *ordo*：次序、编排） |
| 中文名 | 学习任务编排器 |
| 作业原型 | 选题 01「学习任务安排系统」，主要方法：规则判断与优先级排序，难度档：入门 |
| 命名空间 | `ordo` |
| 可执行文件 | `ordo`（Windows 下为 `bin/ordo.exe`） |
| 版本号方案 | 语义化版本 `MAJOR.MINOR.PATCH`，当前 `0.2.0` |
| 许可证 | MIT（见 `LICENSE`） |

**为什么叫 Ordo**：本题的内核不是"存任务"，而是"给任务排序与安放"。
"编排"是一次完整决策——排顺序、塞时间、报冲突——比"清单"更准确地描述系统边界。

---

## 2. 目标与范围

### 2.1 长期目标（毕业前持续维护）

Ordo 不是一次性的作业程序，而是一个**每学期换数据、不换代码**的个人调度工具：

1. **数据自增长**：每天运行一次，`log.csv` 就多一行；学期结束可用它复盘。
2. **换领域只换数据**：换课程、换作息，只改 `data/*.csv` 与 `config.ini`。
3. **单机、纯文本、零依赖**：一个 exe + 几个 csv，十年后仍能编译运行。
4. **算法可替换**：优先级函数、装箱策略都是可注入的接口，后面能换更强的实现而不动上层。

### 2.2 本期交付范围（课程作业）

严格按下表交付，**先完成"必须"，再谈"可选"**：

| 作业要求（原文要点） | 本设计的落点 | 里程碑 |
| --- | --- | --- |
| 清楚的**操作菜单**，数据录入、查询、修改、统计 | CLI 命令分发器 + 命令集 | M1 / M3 |
| 数据**保存到文件**，重新运行可读取 | `CsvTaskRepository` 持久化 | M1 |
| 空数据、输入错误、重复记录、文件不存在要**适当处理** | `Result<T>` / `ErrorCode` 统一返回 | M1 |
| **函数与类组织**，菜单交互、数据处理、核心算法**分离** | 四层架构 + 单向依赖（§4） | M0–M2 |
| **核心算法自行编写** | `algo/` 全部自研，不引第三方 | M1–M3 |
| 规则判断与**优先级排序** | 加权打分 + EDF 双策略 | M1 |
| 准备**测试数据**与**实验报告** | `data/samples/` + `docs/report/` | M4 |
| 演示：启动→完整流程→保存与重新加载 | `docs/report/demo.md` 演示脚本 | M4 |

### 2.3 明确不做（防烂尾清单）

- ~~不做图形界面~~ → **已调整（ADR-0002）**：加入基础 Win32 GUI（M1.5），
  但只做 Windows 原生控件；外部 GUI 通过 `ui::AppModel` 接入，不做自绘动画与复杂视觉。
- 不做数据库、不做网络同步、不做多用户。
- 不做自然语言录入（"下周三前写完高数"这类解析留到 v2 以后，且必须先有 v1）。
- 不做跨平台打包；保证源码在 GCC/Clang/MSVC 下可编译即可。

---

## 3. 术语表

| 术语 | 含义 |
| --- | --- |
| **任务 Task** | 一件有截止时间、有预估耗时、有重要度的待办事项 |
| **截止 Deadline** | 任务必须完成的绝对时间点 |
| **重要度 Importance** | 1–5 的整数权重，由用户给定 |
| **空闲时段 FreeSlot** | 用户可支配的一段时间区间 `[begin, end)` |
| **时间块 Block** | 一次编排结果：把某个任务的一局部安排在某个区间 |
| **编排 Plan** | 若干 Block 的集合，外加告警列表 |
| **紧急度 Urgency** | 由剩余时间与预估时长推导的派生量，不存储 |
| **冲突 Conflict** | 同一区间被两个 Block 占用，或任务预估时长超出剩余可用时间 |
| **L 层** | 调用层级，见 §7 |

---

## 4. 分层架构

四层，依赖**严格单向向下**；上层不知道下层的具体实现，只认接口。

```
┌─────────────────────────────────────────────────────────┐
│  L1  cli/          命令解析与菜单交互                     │
│      只做：读入参 → 调服务 → 打印结果                     │
│      禁止：直接读写文件、写算法                            │
└───────────────────────────┬─────────────────────────────┘
                            │ 调用
┌───────────────────────────▼─────────────────────────────┐
│  L2  app/          应用服务：编排一次业务动作              │
│      只做：取数据 → 调算法 → 落盘 → 组装结果              │
│      禁止：解析命令行、实现具体算法                        │
└───────────┬───────────────────────────────┬─────────────┘
            │ 调用                          │ 调用（依赖注入）
┌───────────▼───────────────┐   ┌───────────▼─────────────┐
│  L3  algo/   纯函数算法层  │   │  L4  io/   持久化层      │
│  输入数据 → 输出结果        │   │  只认接口 ITaskRepository│
│  无 I/O、无全局状态、可单测 │   │  具体实现可替换           │
└───────────────────────────┘   └─────────────────────────┘
```

**为什么这样切**：作业明确要求"菜单交互、数据处理与核心算法分开"。切成四层后，
`algo/` 全部是**纯函数**（给定输入必得同一输出），可以在没有文件、没有菜单的情况下单测；
`io/` 藏在接口后面，以后从 CSV 换成 JSON 只改一个文件。这两点是"长期可维护"的全部技术含义。

**M1.5 补充（ADR-0002）**：L1 有两个适配器——`cli/` 直接调用 `app/`；
`gui/` 经 `ui/`（GUI 无关的界面模型）调用 `app/`。`ui/` 只做表单解析与展示映射，
不放业务规则；`gui/` 只做窗口与控件，不碰文件。因此依赖方向是：

```
cli/ ──────────────┐
                   ├──▶ app/ ──▶ { algo/, io/ }
gui/ ──▶ ui/ ──────┘
```

---

## 5. 文件预期架构

### 5.1 图例

| 标记 | 含义 |
| --- | --- |
| `[M0]` | 骨架阶段创建，M0 结束即可编译运行 |
| `[M1]` | 基本要求阶段创建（作业必须项） |
| `[M2]` | 编排阶段创建（时间装箱与冲突检测） |
| `[M3]` | 长期阶段创建（日志、统计、间隔重复） |
| `[M4]` | 交付阶段创建（测试数据、报告、演示） |
| `[—]` | 目录占位，随内容一并创建 |

`include/ordo/` 与 `src/` **一一镜像**：每个头文件对应一个同名 `.cpp`，无例外。

### 5.2 目录树

```
project_basic_cpp/
├── README.md                        [M0]  项目门面：定位、构建、文档索引
├── LICENSE                          [M0]  MIT 许可证
├── CHANGELOG.md                     [M0]  变更日志（Keep a Changelog 格式）
├── CONTRIBUTING.md                  [M0]  提交规范、代码风格、测试要求
├── Makefile                         [M0]  唯一构建入口（all / test / clean / fmt）
├── .gitignore                       [M0]
├── .editorconfig                    [M0]  缩进、编码、换行统一
├── .clang-format                    [M0]  格式化规则（LLVM 基底 + 4 空格）
│
├── data/                                  运行期数据（受版本控制，便于复现）
│   ├── config.ini                   [M1]  作息配置：每日可用时段、默认时长
│   ├── tasks.csv                    [M1]  任务表（主数据）
│   ├── log.csv                      [M3]  执行日志：每次运行追加一行
│   └── samples/                     [M4]  测试数据集
│       ├── basic.csv                [M4]  正常样例（≥20 条）
│       ├── edge-empty.csv           [M4]  空表
│       ├── edge-duplicate.csv       [M4]  重复 id
│       ├── edge-badvalue.csv        [M4]  非法字段
│       └── overload.csv             [M4]  超载（任务总量 > 可用时间）
│
├── docs/
│   ├── OUTLINE.md                   [M0]  ← 本文件
│   ├── DATA-FORMAT.md               [M1]  字段定义、编码、边界值（§9 的展开）
│   ├── GUI-INTERFACE.md             [M1.5] 外部 GUI 接入契约（AppModel）
│   ├── adr/
│   │   ├── 0001-architecture.md     [M0]  架构决策记录第 1 号
│   │   ├── 0002-gui-layer.md        [M1.5] GUI 适配器与界面模型层
│   │   └── 0003-planning.md         [M2]  任务类型、活动固定时间与编排语义
│   └── report/                      [M4]  实验报告与演示脚本
│       ├── report.md                [M4]  作业要求的实验报告
│       └── demo.md                  [M4]  演示流程脚本（逐条命令）
│
├── include/ordo/
│   ├── version.h                    [M0]  版本宏
│   │
│   ├── core/                              纯数据与基础设施，不依赖任何上层
│   │   ├── types.h                  [M1]  TaskId / Minutes / TimePoint 等别名
│   │   ├── result.h                 [M1]  Result<T>、Status、ErrorCode
│   │   ├── task.h                   [M1]  Task 实体与不变量校验
│   │   ├── slot.h                   [M2]  FreeSlot / Block 及其区间运算
│   │   └── schedule.h               [M2]  Plan 结构与告警类型
│   │
│   ├── io/                                持久化，只暴露接口
│   │   ├── repository.h             [M1]  ITaskRepository 抽象基类
│   │   ├── csv.h                    [M1]  CSV 行切分/转义/字段解析（纯函数）
│   │   ├── config.h                 [M2]  config.ini 解析（作息与装箱策略）
│   │   └── file_repository.h        [M1]  CsvTaskRepository 具体实现
│   │
│   ├── algo/                              纯函数算法，无 I/O
│   │   ├── priority.h               [M1]  加权打分（重要度/紧急度/预估）
│   │   ├── edf.h                    [M1]  最早截止优先排序
│   │   ├── packer.h                 [M2]  时间块装箱（把任务塞进空闲时段）
│   │   ├── conflict.h               [M2]  冲突检测与告警生成
│   │   └── sm2.h                    [M3]  间隔重复（SM-2）复习调度
│   │
│   ├── app/                               应用服务，串联算法与存储
│   │   ├── context.h                [M1]  运行期上下文：仓库引用、配置、时钟
│   │   ├── task_service.h           [M1]  增删改查 + 统计
│   │   └── plan_service.h           [M2]  编排：空闲时段 → 装箱 → 冲突告警
│   │
│   ├── cli/                               交互层（命令行适配器）
│   │   ├── command.h                [M1]  Command 抽象基类
│   │   ├── dispatcher.h             [M1]  命令注册表与分发
│   │   ├── menu.h                   [M1]  文本菜单与输入校验
│   │   └── commands/
│   │       ├── task_cmd.h           [M1]  task add/list/done/edit/remove
│   │       ├── plan_cmd.h           [M2]  plan today/week
│   │       └── stats_cmd.h          [M3]  stats report
│   │
│   ├── ui/                                界面模型（GUI 无关，ADR-0002）
│   │   └── app_model.h              [M1.5] 表单解析、展示行、查询与统计入口
│   │
│   └── gui/                               Win32 适配器（仅 Windows）
│       ├── win32_util.h             [M1.5] UTF-8/UTF-16、DPI 缩放、字体
│       ├── task_dialog.h            [M1.5] 新增/编辑任务对话框
│       └── main_window.h            [M1.5] 主窗口、列表、筛选与统计面板
│
├── src/                                  与 include/ordo 镜像（略列同名 .cpp）
│   ├── main.cpp                     [M0]  唯一入口：装配依赖、启动分发器
│   └── gui/main_gui.cpp             [M1.5] GUI 入口：装配仓库与主窗口
│
└── tests/
    ├── test_framework.h             [M0]  极简断言宏（零第三方依赖）
    ├── test_main.cpp                 [M0]  测试入口：注册并运行所有用例
    ├── test_result.cpp              [M1]  Result/ErrorCode 行为
    ├── test_types.cpp               [M1]  时间字符串解析/格式化、trim 边界
    ├── test_csv.cpp                 [M1]  含逗号、引号、空字段、BOM
    ├── test_file_repository.cpp     [M1]  CSV 往返、损坏文件、原子替换
    ├── test_priority.cpp            [M1]  排序稳定性与并列规则
    ├── test_task_service.cpp        [M1]  假仓库驱动的业务规则
    ├── test_app_model.cpp           [M1.5] 表单解析与展示行映射
    ├── test_packer.cpp              [M2]  恰好装满、超载、零时长、活动占位
    ├── test_config.cpp              [M2]  INI 缺省、非法值、时间解析
    ├── test_plan_service.cpp        [M2]  空闲时段与今日/本周编排
    └── test_sm2.cpp                 [M3]  间隔序列与边界
```

### 5.3 文件数量预算

| 阶段 | 新增源文件上限 | 理由 |
| --- | --- | --- |
| M0 | 3 | 只验证工具链 |
| M1 | 约 14 | 基本要求闭环，够用即止 |
| M2 | 约 10 | 编排是唯一新算法域；含配置读取与三个测试文件 |
| M3 | 约 4 | 统计与复习是增量 |
| M4 | 0 | 只写数据与文档 |

**超出预算的新文件必须先在 §5.2 登记**。这条规则是为了防止"每个功能开一个新文件"式的目录膨胀。

---

## 6. 领域模型

定义在 `core/`，全部为**值语义**，可自由拷贝，不含行为逻辑以外的副作用。

```cpp
namespace ordo {

// ---- core/types.h ----
using TaskId   = std::uint64_t;
using Minutes  = std::int32_t;               // 时长与偏移的统一单位：分钟
using Clock    = std::chrono::system_clock;
using TimePoint = Clock::time_point;

// ---- core/task.h ----
enum class TaskState { Todo, Doing, Done, Dropped };

// 任务类型决定打分系数与装箱方式（ADR-0003）。
enum class TaskKind { CourseWork, SelfStudy, Activity, Review, Other };

struct Task {
    TaskId      id{};                        // 唯一，> 0
    std::string title;                       // 非空
    std::string course;                      // 可为空
    int         importance{3};               // 1..5
    Minutes     estimate{0};                 // 预估耗时，>= 0
    TimePoint   deadline{};                  // 截止时刻
    TaskState   state{TaskState::Todo};
    int         review_stage{0};             // SM-2 阶段，0 表示未进入复习
    std::optional<TimePoint> last_review;    // 上次复习时刻
    TaskKind    kind{TaskKind::Other};       // 业务类型；旧数据默认 other
    std::optional<TimePoint> fixed_start;    // 仅 Activity：固定开始时刻

    // 不变量校验：失败返回 ErrorCode，不做部分修复
    Status validate() const;
};

// ---- core/slot.h ----
struct FreeSlot { TimePoint begin; TimePoint end; };   // 半开区间 [begin, end)
struct Block    { TaskId task; TimePoint begin; TimePoint end; };

// ---- core/schedule.h ----
struct Warning { TaskId task; std::string reason; };
struct Plan {
    std::vector<Block>   blocks;
    std::vector<Warning> warnings;           // 未排上的任务及原因
    Minutes total_assigned{0};
    Minutes total_free{0};
};

} // namespace ordo
```

**关键设计约束**：

- `Task` 只存**事实**（截止时间、预估、重要度），不存**派生量**（紧急度、优先级分数）。
  派生量每次现算——因为"现在几点"在变，存下来必然过期。
- `Plan` 里必须有 `warnings`。排不上不是错误，是**正常业务结果**，要能解释给用户听。
- 所有时间用绝对时刻，不用"第几周第几节"，避免学期切换时数据失效。

---

## 7. 接口与调用类型

### 7.1 四类调用（本设计的核心约定）

| 类型 | 调用方 → 被调方 | 形态 | 允许 I/O | 可单测 | 失败表达 |
| --- | --- | --- | --- | --- | --- |
| **C1 命令调用** | `main` → `Dispatcher` → `Command` | 虚函数，多态 | 仅 stdin/stdout | 否（集成测） | `int` 退出码 |
| **C2 服务调用** | `Command` → `Service` | 普通成员函数 | 间接（经仓库） | 是（假仓库） | `Result<T>` |
| **C3 算法调用** | `Service` → `algo/` | **自由函数**，无状态 | **禁止** | 是（首选） | 返回值 + 结果字段 |
| **C4 存储调用** | `Service` → `ITaskRepository` | 纯虚接口，依赖注入 | 是 | 是（内存实现） | `Result<T>` / `Status` |

**硬规则**：

1. C1 只能调 C2。命令里出现 `std::ifstream` 即为违规。
2. C3 层函数**不得**接受任何 I/O 对象、不得读时钟、不得抛异常。时间由调用方作为参数传入。
3. C4 的具体实现只允许在 `main.cpp` 构造一次，向下以引用传递（依赖注入）。
4. 跨层**不允许抛异常**；异常只在穿越 `main` 时统一捕获并转为退出码。

### 7.2 错误与返回值约定

```cpp
// ---- core/result.h ----
enum class ErrorCode {
    Ok = 0,
    NotFound,        // 记录或文件不存在
    Duplicate,       // id 重复
    InvalidFormat,   // 字段格式错误
    InvalidValue,    // 数值越界（如 importance 不在 1..5）
    EmptyInput,      // 输入为空
    IoError,         // 读写失败
    Corrupted,       // 数据文件结构损坏
};

struct Status {
    ErrorCode   code{ErrorCode::Ok};
    std::string message;                 // 面向用户的中文说明
    bool ok() const noexcept { return code == ErrorCode::Ok; }
    static Status success();
    static Status fail(ErrorCode, std::string);
};

template <typename T>
class Result {
public:
    static Result ok(T value);
    static Result fail(ErrorCode, std::string);
    bool  has_value() const noexcept;
    T&    value();                       // 仅在 has_value() 时调用
    const Status& status() const noexcept;
private:
    std::optional<T> value_;
    Status           status_;
};
```

**为什么不用异常**：作业要求"对空数据、输入错误、重复记录、文件不存在作适当处理"——
这些是**可预期的业务分支**，不是异常情况。用 `Result<T>` 强制调用方处理，
编译器会帮你把漏判的分支暴露出来。

### 7.3 C4 存储层接口

```cpp
// ---- io/repository.h ----
class ITaskRepository {
public:
    virtual ~ITaskRepository() = default;

    virtual Result<std::vector<Task>> load_all() const = 0;
    virtual Status                    save_all(const std::vector<Task>&) = 0;
    virtual Result<Task>              find(TaskId) const = 0;
    virtual Status                    upsert(const Task&) = 0;
    virtual Status                    erase(TaskId) = 0;
};
```

`CsvTaskRepository` 的构造参数为数据文件路径；`save_all` 采用**写临时文件再原子替换**
（`tasks.csv.tmp` → `tasks.csv`），避免中途崩溃导致数据全毁。

### 7.4 C3 算法层接口（全部为自由函数）

```cpp
// ---- algo/priority.h ----
struct ScoreContext { TimePoint now; Minutes daily_capacity; };

// 打分函数可替换：默认加权，后续可换别的模型
using ScoreFn = double (*)(const Task&, const ScoreContext&);

double score_weighted(const Task&, const ScoreContext&);   // 默认实现
std::vector<TaskId> rank_by_score(const std::vector<Task>&, ScoreFn, const ScoreContext&);

// ---- algo/edf.h ----
std::vector<TaskId> rank_by_edf(const std::vector<Task>&, TimePoint now);

// ---- algo/packer.h ----
struct PackPolicy {
    bool   allow_split{true};      // 是否允许把任务拆成多个块
    Minutes min_block{15};         // 最小块时长，低于此值不单独成块
    Minutes gap{10};               // 块间休息
};
struct PackResult { std::vector<Block> blocks; std::vector<Warning> warnings; };

// tasks 必须已按优先级排好序（rank_by_score / rank_by_edf 的输出）。
// Activity 按 fixed_start 整块占位，不参与顺序装箱（ADR-0003）。
PackResult pack(const std::vector<Task>&, const std::vector<FreeSlot>&,
                const PackPolicy&, TimePoint now);

// ---- algo/conflict.h ----
std::vector<Warning> detect_conflicts(const std::vector<Block>&);

// ---- algo/sm2.h ----
struct Sm2State { int stage; double ease; Minutes interval; };
Sm2State sm2_next(const Sm2State&, int quality /*0..5*/, TimePoint now);
```

**纯函数的意义**：`pack()` 不读文件、不问系统时间，给定同样的
（任务表、空闲时段、策略、当前时刻）必然得到同一份结果。这是能写出可靠测试的前提，
也是"核心算法自己写"这条要求唯一站得住的实现方式。

### 7.5 C2 服务层

```cpp
// ---- app/task_service.h ----
class TaskService {
public:
    explicit TaskService(ITaskRepository& repo);

    Result<Task>     add(Task draft);                    // 分配 id、校验、落盘
    Status           update(TaskId, const Task& patch);
    Status           remove(TaskId);
    Result<Task>     get(TaskId) const;
    Result<std::vector<Task>> list(std::optional<TaskState>) const;
    Result<Stats>    stats(TimePoint now) const;         // 计数、超期数、总时长
private:
    ITaskRepository& repo_;
};

// ---- app/plan_service.h ----
class PlanService {
public:
    PlanService(ITaskRepository&, ScheduleConfig);
    Result<Plan> plan_today(TimePoint now, RankMode = RankMode::Score) const;
    Result<Plan> plan_week(TimePoint now, RankMode = RankMode::Score) const;
    // 内部流程：取任务 → 算空闲时段 → 排序 → 装箱 → 冲突检测
};
```

服务层是**唯一**同时知道"数据从哪来"和"算法怎么用"的地方。它不做格式转换，
不做打印，不解析参数。

### 7.6 C1 命令层

```cpp
// ---- cli/command.h ----
struct Args { std::vector<std::string> positional; std::map<std::string,std::string> flags; };

class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view name()    const noexcept = 0;   // "task.add"
    virtual std::string_view help()    const noexcept = 0;   // 一行帮助
    virtual int execute(const Args&, AppContext&) = 0;       // 返回退出码
};

// ---- cli/dispatcher.h ----
class Dispatcher {
public:
    void   add(std::unique_ptr<Command>);
    int    run(const Args&, AppContext&);        // 未命中返回 2
    void   print_help() const;
};
```

命令名采用**点分两级**：`task.add`、`task.list`、`plan.today`、`stats.week`。
既支持 `ordo task add "高数作业" --due 2026-10-30 --est 90`
（一次性命令行，便于脚本化），也支持 `ordo` 无参数进入交互菜单。
两者共用同一套 `Command`，不写两遍。

### 7.7 一次完整调用的时序

```
$ ordo plan today
      │
      ▼
Dispatcher ──C1──▶ PlanCommand::execute
                        │
                        ▼ ──C2──▶ PlanService::plan_today(now)
                                     │
                        ┌────────────┼─────────────┐
                        ▼ C4         ▼ C3          ▼ C3
                 repo.load_all()  rank_by_score  pack(...)
                        │
                        ▼
                  返回 Result<Plan> ──▶ 命令打印 ──▶ 退出码 0
```

这条链路是**唯一**的连接方式。任何绕过它的数据流都属于设计违规。

---

## 8. 实现方式

### 8.1 语言与工具链

| 项 | 决定 | 说明 |
| --- | --- | --- |
| 标准 | **C++17** | 需要 `std::filesystem`、`std::optional`、`std::string_view` |
| 编译器（主） | g++ 16.2.0（MinGW-w64，`C:\Users\Jerry\.vscode\mingw64`） | 已在本机验证 |
| 兼容目标 | GCC / Clang / MSVC | 不使用 GCC 专有扩展 |
| 构建 | **GNU Make**（`Makefile`） | 本机无 cmake；Windows 下命令为 `mingw32-make`（本机无 `make`） |
| 依赖 | **零第三方** | 含测试框架（自研 20 行断言宏） |
| 字符集 | 源码与数据统一 UTF-8（无 BOM） | Windows 控制台需 `chcp 65001` |
| 换行 | LF（`.gitattributes` 统一） | |

### 8.2 代码风格

- 命名：类型 `PascalCase`，函数与变量 `snake_case`，成员变量尾随 `_`，常量 `kPascalCase`。
- 头文件一律 `#pragma once`，且**自包含**（单独 include 可编译）。
- 一律使用 `std::` 前缀，禁止 `using namespace std;`。
- 行长 100 列，4 空格缩进，`{` 不换行（由 `.clang-format` 强制）。
- 编译告警：`-Wall -Wextra -Wpedantic`，M4 前清零。

### 8.3 时间处理约定

- 内部一律用 `TimePoint`（UTC 语义），只在**输入输出边界**转换为本地时间字符串。
- 时间字符串格式固定为 `YYYY-MM-DD HH:MM`，不做模糊解析。
- "今天"由调用方传入 `now`，算法层不得自行取时钟——这是可测试性的硬要求。

### 8.4 持久化约定

- 读：整表读入内存，量级为百条，无需索引。
- 写：写 `*.tmp` → `std::filesystem::rename` 原子替换。
- 失败：任何一步失败都不允许留下半截文件；`tasks.csv` 始终是**上一份完整数据**。
- 首次运行：文件不存在视为空表，自动生成表头，不报错（作业要求"文件不存在要适当处理"）。

---

## 9. 数据文件规范

### 9.1 `data/tasks.csv`

| 序号 | 字段 | 类型 | 约束 | 示例 |
| --- | --- | --- | --- | --- |
| 1 | `id` | uint64 | 唯一，> 0 | `1` |
| 2 | `title` | string | 非空，含逗号需加双引号 | `"高数,第3章习题"` |
| 3 | `course` | string | 可空 | `高等数学` |
| 4 | `importance` | int | 1–5 | `4` |
| 5 | `estimate` | int | ≥ 0，单位分钟 | `90` |
| 6 | `deadline` | datetime | `YYYY-MM-DD HH:MM` | `2026-10-30 23:59` |
| 7 | `state` | enum | `todo` / `doing` / `done` / `dropped` | `todo` |
| 8 | `review_stage` | int | ≥ 0 | `0` |
| 9 | `kind` | enum | `coursework` / `selfstudy` / `activity` / `review` / `other` | `other` |
| 10 | `fixed_start` | datetime | 仅 `activity` 必填，其余留空 | `2026-10-28 19:00` |

首行为表头。字段顺序固定，新增字段**只能追加到行尾**（保证旧文件可读）。
读取时同时接受旧 8 列表头（`kind` 默认 `other`、`fixed_start` 为空）与新 10 列表头；
保存时统一写新表头。

### 9.2 `data/config.ini`

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

### 9.3 `data/log.csv`

| 字段 | 含义 |
| --- | --- |
| `timestamp` | 运行时刻 |
| `command` | 执行的命令名 |
| `task_id` | 关联任务（可空） |
| `minutes` | 实际投入（`task.done --spent 45`） |
| `note` | 备注 |

### 9.4 编码与边界

- UTF-8 无 BOM；读取时容忍 BOM（剥掉首三字节）。
- 行尾 `\n`；读取时容忍 `\r\n`。
- 空行跳过；全空文件视为空表。
- 字段数不符 → `ErrorCode::Corrupted`，**整表拒绝加载**，不静默丢弃行
  （宁可报错，也不要让用户以为数据还在）。

---

## 10. 里程碑与验收对照

| 里程碑 | 目标 | 完成判据 | 对应作业条目 |
| --- | --- | --- | --- |
| **M0 骨架** | 仓库可编译、可测试、可运行 | `mingw32-make && mingw32-make test` 全绿；`ordo --version` 输出正确 | 工程组织 |
| **M1 基本要求** | 任务 CRUD + 持久化 + 两种排序 + 菜单 | 录入 20 条、重启进程数据仍在；非法输入有明确提示；`task.list --rank score` 排序正确 | 录入/查询/修改/统计、文件保存、异常处理、优先级排序 |
| **M1.5 图形界面** | Win32 基础 GUI + GUI 无关的 `ui::AppModel` | `bin/ordo-gui.exe` 可完成增删改查、完成、筛选、排序、统计；不显示假计划数据；`mingw32-make test` 全绿 | 图形化交互（用户新增要求，ADR-0002） |
| **M2 编排** | 任务类型 → 空闲时段 → 装箱 → 冲突告警 | `plan today` 输出时间表；超载时给出每条未排任务的 `Warning`；GUI 计划区显示真实时间块；活动按固定开始时间整块占位 | 规则判断、时间安排 |
| **M3 长期能力** | 日志、统计、SM-2 复习 | `stats.week` 输出完成率与投入分布；复习任务按间隔出现在列表 | 统计、拓展 |
| **M4 交付** | 测试数据、报告、演示 | `docs/report/report.md` 含至少 1 个失败案例分析与已知问题 | 测试数据、实验报告、演示 |

**里程碑纪律**：一个里程碑未验收，不开启下一个。M2 之前不写任何 SM-2 代码。
M1.5 先于 M2（ADR-0002）：编排算法的接口要围绕 `ui::AppModel` 的计划视图设计。

**当前进度（2026-10-25）**：M0、M1、M1.5、M2 已完成，`mingw32-make test` 共 86 个用例；
下一步 M3（日志、统计、SM-2）。

---

## 11. 测试策略

| 层次 | 手段 | 要求 |
| --- | --- | --- |
| C3 算法 | 单元测试 | 每个算法文件至少 4 个用例，含边界（零时长、空列表、恰好装满、超载） |
| C4 存储 | 单元测试 + 往返测试 | 写入→读取→逐字段相等；损坏文件必须返回 `Corrupted` |
| C2 服务 | 假仓库注入 | 用内存实现替代 CSV，验证业务规则（如重复 id 拒绝） |
| C1 命令 | 集成测试 | 脚本跑完整命令序列，比对 stdout 关键字段 |
| 数据回归 | `data/samples/` | 每个边界样例都有对应命令，结果可人工核对 |

**验收前必须**：`mingw32-make test` 全绿，且 `docs/report/report.md` 中记录**至少一个真实出错案例**
（作业明确要求"分析至少一个出错或效果不理想的案例"）。

---

## 12. 变更流程

1. 想改架构（分层、接口签名、文件职责）→ 先写 ADR 到 `docs/adr/`，编号递增。
2. 想加文件 → 先在本文件 §5.2 登记并分配里程碑。
3. 想加依赖 → 默认否决；确有必要必须在 ADR 中论证收益大于可移植性损失。
4. 提交信息用中文，一次提交只做一类改动。
5. 每次里程碑验收后更新 `CHANGELOG.md`。

---

## 13. 索引

| 文档 | 作用 |
| --- | --- |
| `README.md` | 门面：一句话定位、构建命令、文档地图 |
| `docs/OUTLINE.md` | **本文件**，工程总纲 |
| `docs/CODE-GUIDE.txt` | 全库导读：逐文件、逐行代码说明，面向第一次接触项目的人 |
| `docs/DATA-FORMAT.md` | §9 的完整展开（M1 编写） |
| `docs/GUI-INTERFACE.md` | 外部 GUI 接入契约：`ui::AppModel` 的职责与调用方式 |
| `docs/adr/0001-architecture.md` | 四层架构、构建系统、错误模型的决策记录 |
| `docs/adr/0002-gui-layer.md` | GUI 适配器、界面模型层与 M1.5 的决策记录 |
| `docs/adr/0003-planning.md` | 任务类型、活动固定时间、打分系数与 M2 编排语义 |
| `docs/report/report.md` | 课程实验报告（M4） |
| `docs/report/demo.md` | 演示脚本（M4） |
| `CHANGELOG.md` | 版本变更 |
| `CONTRIBUTING.md` | 提交与风格规范 |

---

*本文件随项目演进；修订须在 `CHANGELOG.md` 留痕。*
