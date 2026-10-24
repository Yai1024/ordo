#pragma once

// C2 应用服务：任务的增删改查与统计（OUTLINE §7.5）。
// 唯一同时知道"数据从哪来"和"算法怎么用"的层；不做格式转换、不打印、不解析参数。

#include <cstddef>
#include <optional>
#include <vector>

#include "ordo/core/result.h"
#include "ordo/core/task.h"
#include "ordo/io/repository.h"

namespace ordo {

enum class RankMode { None, Score, Edf };

struct Stats {
    std::size_t total{0};
    std::size_t todo{0};
    std::size_t doing{0};
    std::size_t done{0};
    std::size_t dropped{0};
    std::size_t overdue{0};      // 未完成且截止时间已过
    Minutes open_estimate{0};    // 未完成任务的预估总时长
    Minutes done_estimate{0};    // 已完成任务的预估总时长
};

class TaskService {
public:
    explicit TaskService(ITaskRepository& repo);

    // 分配 id、校验、落盘。draft.id 为 0 时自动分配；非 0 时重复即拒绝。
    Result<Task> add(Task draft);

    // patch 必须是完整记录（id 与参数一致）；字段级合并由调用方完成后传入。
    Status update(TaskId id, const Task& patch);

    Status remove(TaskId id);
    Result<Task> get(TaskId id) const;

    Result<std::vector<Task>> list(std::optional<TaskState> state) const;

    // 排序入口：排完再返回完整记录，命令层不需要知道 TaskId → Task 的映射。
    Result<std::vector<Task>> list_ranked(std::optional<TaskState> state, RankMode mode,
                                          TimePoint now) const;

    Result<Stats> stats(TimePoint now) const;

private:
    ITaskRepository& repo_;
};

}  // namespace ordo
