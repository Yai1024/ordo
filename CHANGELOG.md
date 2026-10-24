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

### Planned
- M2 编排：空闲时段推导、时间块装箱、冲突告警（界面围绕 `ui::AppModel` 的计划视图设计）
- M3 长期能力：执行日志、周统计、SM-2 间隔复习
- M4 交付：测试数据集、实验报告、演示脚本
