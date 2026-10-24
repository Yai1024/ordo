#include "ordo/algo/edf.h"

#include <algorithm>

namespace ordo {

std::vector<TaskId> rank_by_edf(const std::vector<Task>& tasks, TimePoint now) {
    (void)now;  // EDF 只看截止时刻；参数保留以便与其它策略同形。

    std::vector<const Task*> ordered;
    ordered.reserve(tasks.size());
    for (const Task& task : tasks) {
        ordered.push_back(&task);
    }

    std::stable_sort(ordered.begin(), ordered.end(), [](const Task* left, const Task* right) {
        if (left->deadline != right->deadline) {
            return left->deadline < right->deadline;
        }
        if (left->importance != right->importance) {
            return left->importance > right->importance;
        }
        return left->id < right->id;
    });

    std::vector<TaskId> ids;
    ids.reserve(ordered.size());
    for (const Task* task : ordered) {
        ids.push_back(task->id);
    }
    return ids;
}

}  // namespace ordo
