#include "ordo/app/task_service.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include "ordo/algo/edf.h"
#include "ordo/algo/priority.h"

namespace ordo {
namespace {

template <typename T>
Result<T> forward_failure(const Status& status) {
    return Result<T>::fail(status.code, status.message);
}

}  // namespace

TaskService::TaskService(ITaskRepository& repo) : repo_(repo) {}

Result<Task> TaskService::add(Task draft) {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<Task>(loaded.status());
    }

    std::vector<Task> tasks = std::move(loaded.value());

    if (draft.id == 0) {
        TaskId next_id = 0;
        for (const Task& task : tasks) {
            next_id = std::max(next_id, task.id);
        }
        if (next_id == std::numeric_limits<TaskId>::max()) {
            return Result<Task>::fail(ErrorCode::InvalidValue, "任务 id 已用尽");
        }
        draft.id = next_id + 1;
    } else {
        const bool duplicated =
            std::any_of(tasks.begin(), tasks.end(),
                        [&draft](const Task& task) { return task.id == draft.id; });
        if (duplicated) {
            return Result<Task>::fail(ErrorCode::Duplicate,
                                      "任务 #" + std::to_string(draft.id) + " 已存在");
        }
    }

    const Status check = draft.validate();
    if (!check.ok()) {
        return Result<Task>::fail(check.code, check.message);
    }

    tasks.push_back(draft);
    const Status saved = repo_.save_all(tasks);
    if (!saved.ok()) {
        return Result<Task>::fail(saved.code, saved.message);
    }

    return Result<Task>::ok(std::move(draft));
}

Status TaskService::update(TaskId id, const Task& patch) {
    if (patch.id != id) {
        return Status::fail(ErrorCode::InvalidValue, "补丁的 id 与目标任务不一致");
    }

    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return loaded.status();
    }

    std::vector<Task> tasks = std::move(loaded.value());
    const auto position =
        std::find_if(tasks.begin(), tasks.end(),
                     [id](const Task& task) { return task.id == id; });
    if (position == tasks.end()) {
        return Status::fail(ErrorCode::NotFound, "找不到任务 #" + std::to_string(id));
    }

    const Status check = patch.validate();
    if (!check.ok()) {
        return check;
    }

    *position = patch;
    return repo_.save_all(tasks);
}

Status TaskService::remove(TaskId id) {
    return repo_.erase(id);
}

Result<Task> TaskService::get(TaskId id) const {
    return repo_.find(id);
}

Result<std::vector<Task>> TaskService::list(std::optional<TaskState> state) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<std::vector<Task>>(loaded.status());
    }
    return Result<std::vector<Task>>::ok(filter_by_state(loaded.value(), state));
}

Result<std::vector<Task>> TaskService::list_ranked(std::optional<TaskState> state,
                                                   RankMode mode, TimePoint now) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<std::vector<Task>>(loaded.status());
    }

    std::vector<Task> tasks = filter_by_state(loaded.value(), state);
    if (mode == RankMode::None) {
        return Result<std::vector<Task>>::ok(std::move(tasks));
    }

    std::vector<TaskId> ordered_ids;
    if (mode == RankMode::Score) {
        ordered_ids = rank_by_score(tasks, score_weighted,
                                    ScoreContext{now, kDefaultDailyCapacity});
    } else {
        ordered_ids = rank_by_edf(tasks, now);
    }

    std::unordered_map<TaskId, const Task*> by_id;
    by_id.reserve(tasks.size());
    for (const Task& task : tasks) {
        by_id.emplace(task.id, &task);
    }

    std::vector<Task> ordered;
    ordered.reserve(tasks.size());
    for (const TaskId id : ordered_ids) {
        const auto found = by_id.find(id);
        if (found != by_id.end()) {
            ordered.push_back(*found->second);
        }
    }

    return Result<std::vector<Task>>::ok(std::move(ordered));
}

Result<Stats> TaskService::stats(TimePoint now) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<Stats>(loaded.status());
    }

    Stats stats;
    for (const Task& task : loaded.value()) {
        ++stats.total;
        switch (task.state) {
            case TaskState::Todo:
                ++stats.todo;
                break;
            case TaskState::Doing:
                ++stats.doing;
                break;
            case TaskState::Done:
                ++stats.done;
                break;
            case TaskState::Dropped:
                ++stats.dropped;
                break;
        }

        if (is_open(task.state)) {
            stats.open_estimate += task.estimate;
            if (task.deadline < now) {
                ++stats.overdue;
            }
        } else if (task.state == TaskState::Done) {
            stats.done_estimate += task.estimate;
        }
    }

    return Result<Stats>::ok(stats);
}

}  // namespace ordo
