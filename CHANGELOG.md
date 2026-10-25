# Changelog

本项目的所有重要变更记录于此。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

## [Unreleased]

### Added
- 2026-10-24 · **M0 骨架**
  - 建立仓库结构：`include/ordo/`、`src/`、`tests/`、`data/`、`docs/`
  - `docs/OUTLINE.md`：工程总纲（分层架构、文件清单、调用类型、数据规范、里程碑）
  - `docs/CODE-GUIDE.txt`：全库导读（逐文件、逐行代码说明，面向纯新手）
  - `docs/adr/0001-architecture.md`：首批架构决策
  - `Makefile`：`all` / `test` / `clean` / `fmt` 四个目标
  - `.editorconfig`、`.clang-format`、`.gitattributes`、`.gitignore`
  - `README.md`、`CONTRIBUTING.md`、`LICENSE`（MIT）
  - `include/ordo/version.h`、`src/main.cpp`：可编译的最小可执行体

- 2026-10-24 · **M1 基本要求**
  - `core/`：`Result<T>` / `Status` / `ErrorCode`、`Task` 与不变量校验、时间与文本工具
  - `io/`：RFC 4180 CSV 切分/转义、`CsvTaskRepository`（临时文件 + 原子替换）
  - `algo/`：加权打分 `score_weighted` / `rank_by_score`、最早截止优先 `rank_by_edf`
  - `app/`：`TaskService` 增删改查、排序入口与统计；`AppContext` 注入仓库、流与时钟
  - `cli/`：命令注册表、六个 `task.*` 命令、文本交互菜单
  - `main.cpp`：依赖装配、UTF-8 控制台、Windows 宽命令行参数转 UTF-8
  - 测试：41 个用例（core / CSV / 仓库往返 / 排序 / 服务）
  - 文档：新增 `docs/DATA-FORMAT.md`
  - 构建：静态链接 C++ 运行库，产物 exe 不依赖 PATH 中的 MinGW DLL

- 2026-10-24 · **M1.5 图形界面**
  - `ui/AppModel`：GUI 无关的界面模型（表单解析、展示行、查询与统计）
  - `gui/`：Win32 主窗口、任务对话框、DPI 缩放与 UTF-8/UTF-16 转换
  - 新增产物 `bin/ordo-gui.exe`：增删改查、状态筛选、两种排序、实时统计
  - 右侧预留"今日计划"区域，M2 接入后显示时间块
  - `Makefile` 增加 `gui` / `run-gui`；非 Windows 平台仍只构建 CLI
  - 文档：新增 `docs/adr/0002-gui-layer.md`、`docs/GUI-INTERFACE.md`
  - 测试：49 个用例（新增 AppModel 的 8 个表单与展示映射用例）

- 2026-10-25 · **M2 编排**
  - `core/`：`TaskKind`（课程作业 / 自主学习 / 活动 / 复习考试 / 其他）与
    `fixed_start`；`FreeSlot` / `Block` 区间运算；`Plan` / `Warning`
  - `io/config.h`：零依赖 INI 解析，缺省值、非法值行号、BOM/CRLF 容忍
  - `io/file_repository`：兼容旧 8 列与新 10 列 CSV，保存时自动迁移
  - `algo/packer`：按优先级贪心装箱，支持拆分、`gap`、`min_block`、
    活动固定占位、超期告警
  - `algo/conflict`：时间块重叠检测，对冲突双方各给一条告警
  - `algo/priority`：类型系数（作业 1.15 / 复习 1.10 / 自学 0.90）
  - `app/plan_service`：今日（填满空闲时间）与本周（滚动 7 天、按截止过滤）编排
  - `cli`：`plan today` / `plan week`，菜单新增今日/本周计划；
    `task add/edit` 支持 `--kind` / `--start`
  - `ui/AppModel`：`plan_today()` / `plan_week()` 返回 `PlanView`；
    `TaskRow` / `TaskForm` 增加类型与开始时间
  - `gui`：列表新增类型列；右侧计划区改为可滚动只读文本框；
    对话框新增类型下拉，活动时"截止时间"切换为"开始时间"
  - 文档：`docs/adr/0003-planning.md`、OUTLINE / DATA-FORMAT / GUI-INTERFACE 同步
  - 测试：86 个用例（新增 packer / config / plan_service 与类型相关用例）

- 2026-10-25 · **M3 长期能力**
  - `io/event_log.h/.cpp`：`IEventLog` / `CsvEventLog`，写操作追加到 `data/log.csv`，
    表头 `timestamp,command,task_id,minutes,note`，CLI 与 GUI 共用同一套事件名
  - `core/task` + `io/file_repository`：追加 `ease_factor` / `interval_days` /
    `last_review` 三列（13 列），兼容 8/10/13 列表头并在保存时迁移
  - `algo/sm2`：标准 SM-2 质量 0–5、难度系数 `[1.3, 5.0]`、间隔上限 365 天，
    以及到期判断与剩余天数
  - `app/task_service`：`set_state` / `grade_review` / `due_reviews` / `weekly_stats`；
    写操作统一落日志，日志失败不阻断业务
  - `cli`：新增 `task review`、`stats week`、`task done --spent`；列表显示复习状态；
    菜单新增“复习到期”“本周统计”
  - `ui/AppModel` + `gui`：复习状态列、复习评分弹窗、右侧本周统计；
    GUI 与 CLI 写同一份 `log.csv`
  - `docs/adr/0004-tracking.md`；OUTLINE / DATA-FORMAT / GUI-INTERFACE / CODE-GUIDE 同步
  - 测试：117 个用例（新增 SM-2、事件日志、周统计、13 列迁移与复习界面模型用例）

### Planned
- M4 交付：测试数据集、实验报告、演示脚本
