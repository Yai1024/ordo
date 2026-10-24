#include "ordo/algo/priority.h"

#include <algorithm>

namespace ordo {
namespace {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

}  // namespace

double score_weighted(const Task& task, const ScoreContext& context) {
    const double capacity =
        static_cast<double>(std::max<Minutes>(1, context.daily_capacity));

    const double importance_span = static_cast<double>(kMaxImportance - kMinImportance);
    const double importance =
        clamp01(static_cast<double>(task.importance - kMinImportance) / importance_span);

    const double minutes_left = static_cast<double>(minutes_between(context.now, task.deadline));

    double urgency = 1.0 / (1.0 + std::max(0.0, minutes_left) / capacity);
    if (minutes_left <= 0.0) {
        // 已超期：紧急度进入 (1, 2]，确保排在所有未超期任务之前。
        urgency = 1.0 + clamp01(-minutes_left / capacity);
    }

    // 短任务优先：同样重要、同样紧急时，先做能做完的。
    const double effort =
        1.0 / (1.0 + static_cast<double>(std::max<Minutes>(0, task.estimate)) / capacity);

    return 100.0 * (0.5 * importance + 0.3 * urgency + 0.2 * effort);
}

std::vector<TaskId> rank_by_score(const std::vector<Task>& tasks, ScoreFn score,
                                  const ScoreContext& context) {
    if (score == nullptr) {
        score = score_weighted;
    }

    struct Entry {
        TaskId id;
        double value;
        TimePoint deadline;
    };

    std::vector<Entry> entries;
    entries.reserve(tasks.size());
    for (const Task& task : tasks) {
        entries.push_back(Entry{task.id, score(task, context), task.deadline});
    }

    std::stable_sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        if (left.value != right.value) {
            return left.value > right.value;
        }
        if (left.deadline != right.deadline) {
            return left.deadline < right.deadline;
        }
        return left.id < right.id;
    });

    std::vector<TaskId> ids;
    ids.reserve(entries.size());
    for (const Entry& entry : entries) {
        ids.push_back(entry.id);
    }
    return ids;
}

}  // namespace ordo
