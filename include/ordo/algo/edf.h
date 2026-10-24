#pragma once

// C3 算法层：最早截止优先（Earliest Deadline First，OUTLINE §7.4）。
// 纯函数：不读文件、不取系统时钟、不抛异常。

#include <vector>

#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {

// 按截止时刻升序；截止相同则重要度高的在前，再相同则 id 小的在前。
// now 由调用方传入，保持与其它排序策略相同的调用形状。
std::vector<TaskId> rank_by_edf(const std::vector<Task>& tasks, TimePoint now);

}  // namespace ordo
