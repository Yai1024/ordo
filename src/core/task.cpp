#include "ordo/core/task.h"

namespace ordo {

Status Task::validate() const {
    if (id == 0) {
        return Status::fail(ErrorCode::InvalidValue, "任务 id 必须大于 0");
    }

    const std::string_view cleaned_title = trim(title);
    if (cleaned_title.empty()) {
        return Status::fail(ErrorCode::EmptyInput, "任务标题不能为空");
    }
    if (title.find_first_of("\r\n") != std::string::npos) {
        return Status::fail(ErrorCode::InvalidValue, "任务标题不能包含换行");
    }
    if (course.find_first_of("\r\n") != std::string::npos) {
        return Status::fail(ErrorCode::InvalidValue, "课程名不能包含换行");
    }

    if (importance < kMinImportance || importance > kMaxImportance) {
        return Status::fail(ErrorCode::InvalidValue, "重要度必须在 1 到 5 之间");
    }
    if (estimate < 0) {
        return Status::fail(ErrorCode::InvalidValue, "预估时长不能为负数");
    }
    if (review_stage < 0) {
        return Status::fail(ErrorCode::InvalidValue, "复习阶段不能为负数");
    }
    if (deadline.time_since_epoch().count() == 0) {
        return Status::fail(ErrorCode::InvalidValue, "截止时间不能为空");
    }

    return Status::success();
}

std::string_view to_string(TaskState state) noexcept {
    switch (state) {
        case TaskState::Todo:
            return "todo";
        case TaskState::Doing:
            return "doing";
        case TaskState::Done:
            return "done";
        case TaskState::Dropped:
            return "dropped";
    }
    return "todo";
}

std::optional<TaskState> parse_task_state(std::string_view text) noexcept {
    if (text == "todo") {
        return TaskState::Todo;
    }
    if (text == "doing") {
        return TaskState::Doing;
    }
    if (text == "done") {
        return TaskState::Done;
    }
    if (text == "dropped") {
        return TaskState::Dropped;
    }
    return std::nullopt;
}

bool is_open(TaskState state) noexcept {
    return state == TaskState::Todo || state == TaskState::Doing;
}

std::vector<Task> filter_by_state(const std::vector<Task>& tasks,
                                  std::optional<TaskState> state) {
    if (!state.has_value()) {
        return tasks;
    }

    std::vector<Task> filtered;
    filtered.reserve(tasks.size());
    for (const Task& task : tasks) {
        if (task.state == *state) {
            filtered.push_back(task);
        }
    }
    return filtered;
}

}  // namespace ordo
