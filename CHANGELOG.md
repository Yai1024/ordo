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

### Planned
- M1 基本要求：任务 CRUD、CSV 持久化、加权排序与 EDF、文本菜单
- M2 编排：空闲时段推导、时间块装箱、冲突告警
- M3 长期能力：执行日志、周统计、SM-2 间隔复习
- M4 交付：测试数据集、实验报告、演示脚本
