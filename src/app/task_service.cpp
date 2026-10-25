#include "ordo/app/task_service.h"

#include <algorithm>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ordo/algo/edf.h"
#include "ordo/algo/priority.h"
#include "ordo/algo/sm2.h"

namespace ordo {
namespace {

template <typename T>
Result<T> forward_failure(const Status& status) {
    return Result<T>::fail(status.code, status.message);
}

TimePoint start_of_today(TimePoint now) {
    const std::string date = format_date(now);
    const std::optional<TimePoint> parsed = parse_datetime(date + " 00:00");
    return parsed.value_or(now);
}

}  // namespace

TaskService::TaskService(ITaskRepository& repo, IEventLog* log) : repo_(repo), log_(log) {}

void TaskService::log_event(TimePoint when, std::string_view command, TaskId id,
                            std::optional<Minutes> minutes, std::string_view note) const {
    if (log_ == nullptr) {
        return;
    }
    LogEntry entry;
    entry.timestamp = when;
    entry.command = std::string{command};
    entry.task_id = id;
    entry.minutes = minutes;
    entry.note = std::string{note};
    // 旁路审计：日志写不进去不阻断业务（ADR-0004 决策 6）。
    (void)log_->append(entry);
}

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

    log_event(Clock::now(), "task.add", draft.id, std::nullopt, draft.title);
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
    const Status saved = repo_.save_all(tasks);
    if (!saved.ok()) {
        return saved;
    }
    log_event(Clock::now(), "task.edit", id, std::nullopt, patch.title);
    return Status::success();
}

Status TaskService::set_state(TaskId id, TaskState state, std::optional<Minutes> spent) {
    auto found = repo_.find(id);
    if (!found.has_value()) {
        return found.status();
    }

    Task task = std::move(found.value());
    if (task.state == state) {
        return Status::success();  // 无变化不写日志
    }

    task.state = state;
    const Status check = task.validate();
    if (!check.ok()) {
        return check;
    }
    const Status saved = repo_.upsert(task);
    if (!saved.ok()) {
        return saved;
    }

    const bool done = state == TaskState::Done;
    log_event(Clock::now(), done ? "task.done" : "task.edit", id,
              done ? spent : std::nullopt, task.title);
    return Status::success();
}

Status TaskService::remove(TaskId id) {
    auto found = repo_.find(id);
    if (!found.has_value()) {
        return found.status();
    }

    const Status erased = repo_.erase(id);
    if (!erased.ok()) {
        return erased;
    }
    log_event(Clock::now(), "task.remove", id, std::nullopt, found.value().title);
    return Status::success();
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

Result<Task> TaskService::grade_review(TaskId id, int quality, TimePoint now,
                                       std::optional<Minutes> spent) {
    auto found = repo_.find(id);
    if (!found.has_value()) {
        return forward_failure<Task>(found.status());
    }

    Task task = std::move(found.value());
    if (task.kind != TaskKind::Review) {
        return Result<Task>::fail(ErrorCode::InvalidValue,
                                  "任务 #" + std::to_string(id) + " 不是复习类，不能评分");
    }

    const Result<Sm2State> next =
        sm2_update(Sm2State{task.review_stage, task.ease_factor, task.interval_days}, quality);
    if (!next.has_value()) {
        return forward_failure<Task>(next.status());
    }

    task.review_stage = next.value().stage;
    task.ease_factor = next.value().ease;
    task.interval_days = next.value().interval_days;
    task.last_review = now;
    // 复习一次后重新打开，等待下一个间隔；否则完成任务会永远从列表消失。
    task.state = TaskState::Todo;

    const Status saved = repo_.upsert(task);
    if (!saved.ok()) {
        return Result<Task>::fail(saved.code, saved.message);
    }

    log_event(now, "task.review", id, spent, "quality=" + std::to_string(quality));
    return Result<Task>::ok(std::move(task));
}

Result<std::vector<Task>> TaskService::due_reviews(TimePoint now) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<std::vector<Task>>(loaded.status());
    }

    std::vector<Task> due;
    for (const Task& task : loaded.value()) {
        if (is_open(task.state) && review_due(task, now)) {
            due.push_back(task);
        }
    }

    const auto due_time = [](const Task& task) {
        if (!task.last_review.has_value()) {
            return TimePoint::min();
        }
        return add_minutes(*task.last_review, task.interval_days * kMinutesPerDay);
    };
    std::sort(due.begin(), due.end(), [&due_time](const Task& left, const Task& right) {
        const TimePoint left_time = due_time(left);
        const TimePoint right_time = due_time(right);
        if (left_time != right_time) {
            return left_time < right_time;
        }
        return left.id < right.id;
    });

    return Result<std::vector<Task>>::ok(std::move(due));
}

Result<WeeklyStats> TaskService::weekly_stats(TimePoint now) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return forward_failure<WeeklyStats>(loaded.status());
    }

    WeeklyStats stats;
    stats.window_end = now;
    stats.window_begin = add_minutes(start_of_today(now), -6 * kMinutesPerDay);

    std::unordered_map<TaskId, TaskKind> kinds;
    kinds.reserve(loaded.value().size());
    std::unordered_set<TaskId> due_open_ids;
    for (const Task& task : loaded.value()) {
        kinds.emplace(task.id, task.kind);
        if (is_open(task.state) && task.deadline >= stats.window_begin &&
            task.deadline <= stats.window_end) {
            due_open_ids.insert(task.id);
        }
    }

    if (log_ == nullptr) {
        stats.expected = due_open_ids.size();
        stats.has_rate = stats.expected > 0;
        return Result<WeeklyStats>::ok(std::move(stats));
    }

    auto entries = log_->read();
    if (!entries.has_value()) {
        return forward_failure<WeeklyStats>(entries.status());
    }

    std::unordered_set<TaskId> completed_ids;
    std::unordered_map<int, Minutes> minutes_by_kind;
    for (const LogEntry& entry : entries.value()) {
        if (entry.timestamp < stats.window_begin || entry.timestamp > stats.window_end) {
            continue;
        }

        if (entry.command == "task.done" && entry.task_id.has_value()) {
            completed_ids.insert(*entry.task_id);
        }

        if (entry.minutes.has_value() && *entry.minutes > 0) {
            const auto found =
                entry.task_id.has_value() ? kinds.find(*entry.task_id) : kinds.end();
            const TaskKind kind =
                found == kinds.end() ? TaskKind::Other : found->second;
            minutes_by_kind[static_cast<int>(kind)] += *entry.minutes;
        }
    }

    stats.completed = completed_ids.size();
    stats.expected = completed_ids.size();
    for (const TaskId id : due_open_ids) {
        if (completed_ids.find(id) == completed_ids.end()) {
            ++stats.expected;
        }
    }
    stats.has_rate = stats.expected > 0;
    if (stats.has_rate) {
        stats.completion_percent =
            static_cast<int>(stats.completed * 100 / stats.expected);
    }

    for (const auto& [kind, minutes] : minutes_by_kind) {
        stats.total_minutes += minutes;
        stats.by_kind.push_back(KindMinutes{static_cast<TaskKind>(kind), minutes});
    }
    std::sort(stats.by_kind.begin(), stats.by_kind.end(),
              [](const KindMinutes& left, const KindMinutes& right) {
                  if (left.minutes != right.minutes) {
                      return left.minutes > right.minutes;
                  }
                  return static_cast<int>(left.kind) < static_cast<int>(right.kind);
              });

    return Result<WeeklyStats>::ok(std::move(stats));
}

}  // namespace ordo
