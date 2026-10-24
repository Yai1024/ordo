#pragma once

// C4 存储层接口（OUTLINE §7.3）。
// 上层只认这个接口；具体实现（CSV、内存、未来的 JSON）可以整体替换。

#include <vector>

#include "ordo/core/result.h"
#include "ordo/core/task.h"

namespace ordo {

class ITaskRepository {
public:
    virtual ~ITaskRepository() = default;

    virtual Result<std::vector<Task>> load_all() const = 0;
    virtual Status save_all(const std::vector<Task>& tasks) = 0;
    virtual Result<Task> find(TaskId id) const = 0;
    virtual Status upsert(const Task& task) = 0;
    virtual Status erase(TaskId id) = 0;
};

}  // namespace ordo
