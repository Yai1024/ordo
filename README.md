# Ordo · 学习任务编排器

**给任务排序、给时间装箱、给冲突告警。**

Ordo 是一个 C++17 编写的单机学习任务调度工具。它不是待办清单——清单只记录，
Ordo 要回答的是：*今晚这几小时，先做哪件、做多久、哪件排不下了*。

> 本项目同时是《高级语言程序设计》综合实验大作业（选题 01 学习任务安排系统）的交付物，
> 但按**可长期维护的个人工具**设计，而非一次性作业。

---

## 状态

| 项 | 值 |
| --- | --- |
| 版本 | `0.1.0`（M1 基本要求） |
| 阶段 | 任务 CRUD、CSV 持久化、加权排序 / EDF、文本菜单已可用 |
| 测试 | 41 个用例全部通过（`mingw32-make test`） |
| 构建 | 通过（MinGW-w64 g++ 16.2.0） |

---

## 快速开始

```bash
mingw32-make            # 编译，产物 bin/ordo.exe
mingw32-make test       # 编译并运行全部单元测试
./bin/ordo --version    # 查看版本
./bin/ordo              # 进入交互菜单
mingw32-make clean      # 清理
```

常用命令：

```bash
# 添加：标题与截止时间必填；截止只写日期时按当天 23:59
./bin/ordo task add "高数第3章习题" --due "2026-10-30 23:59" \
    --course 高等数学 --importance 4 --est 90

./bin/ordo task list                  # 默认只看未完成任务
./bin/ordo task list --state all      # 全部状态
./bin/ordo task list --rank score     # 加权优先级排序
./bin/ordo task list --rank edf       # 最早截止优先
./bin/ordo task done 1
./bin/ordo task edit 1 --importance 5 --due "2026-10-28 12:00"
./bin/ordo task remove 1
./bin/ordo task stats
```

数据默认写入 `data/tasks.csv`；`--data <路径>` 可指定其他文件（便于演示与测试）。
程序启动时会把 Windows 控制台切到 UTF-8，中文输入输出不会乱码。

> **注意**：本机没有 `make`，MinGW 提供的是 `mingw32-make`。在 Linux/macOS 或 WSL 下把
> 上面的 `mingw32-make` 换成 `make` 即可，Makefile 本身是标准 GNU Make 语法。
> 构建入口统一为 `Makefile`，本机未安装 cmake，详见 `docs/adr/0001-architecture.md`。

---

## 文档地图

工程规则**只**写在 `docs/OUTLINE.md` 里，代码注释服从它。

| 文档 | 内容 |
| --- | --- |
| **[`docs/OUTLINE.md`](docs/OUTLINE.md)** | **工程总纲**：分层、文件架构、接口与调用类型、数据规范、里程碑 |
| **[`docs/CODE-GUIDE.txt`](docs/CODE-GUIDE.txt)** | **全库导读**：给第一次打开项目的人，逐文件逐行讲清每个文件与每行代码 |
| [`docs/DATA-FORMAT.md`](docs/DATA-FORMAT.md) | 数据文件规范：字段、编码、边界与错误处理 |
| [`docs/adr/0001-architecture.md`](docs/adr/0001-architecture.md) | 架构决策记录：为什么四层、为什么 Make、为什么不用异常 |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | 提交规范与代码风格 |
| [`CHANGELOG.md`](CHANGELOG.md) | 版本变更 |
| [`LICENSE`](LICENSE) | MIT |

作业原始要求：根目录 `高级语言程序设计综合实验大作业选题.pdf`。

---

## 设计要点（三句话）

1. **四层单向依赖**：`cli → app → {algo, io}`。算法层是纯函数，不碰文件、不问时钟，可独立测试。
2. **错误用返回值而非异常**：空数据、格式错、重复 id 都是可预期的业务分支，用 `Result<T>` 强制处理。
3. **数据是纯文本**：UTF-8 CSV + INI，Atom 里可读、Excel 里可改、十年后仍能打开。

---

## 目录

```
include/ordo/   头文件（与 src/ 一一镜像）
src/            实现
tests/          测试（零第三方依赖）
data/           运行期数据（CSV / INI）
docs/           文档与报告
```

完整文件清单与每个文件的职责见 `docs/OUTLINE.md` §5.2。

---

Copyright © 2026 Jerry · MIT License
