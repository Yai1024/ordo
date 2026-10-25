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

    if (is_fixed_time(kind)) {
        if (!fixed_start.has_value() || fixed_start->time_since_epoch().count() == 0) {
            return Status::fail(ErrorCode::InvalidValue, "活动类任务必须填写固定开始时间");
        }
        if (estimate <= 0) {
            return Status::fail(ErrorCode::InvalidValue, "活动类任务的预估时长必须大于 0");
        }
        if (deadline < add_minutes(*fixed_start, estimate)) {
            return Status::fail(ErrorCode::InvalidValue,
                                "活动类任务的截止时间不能早于开始时间加预估时长");
        }
    } else if (fixed_start.has_value()) {
        return Status::fail(ErrorCode::InvalidValue, "只有活动类任务可以填写固定开始时间");
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

std::string_view to_string(TaskKind kind) noexcept {
    switch (kind) {
        case TaskKind::CourseWork:
            return "coursework";
        case TaskKind::SelfStudy:
            return "selfstudy";
        case TaskKind::Activity:
            return "activity";
        case TaskKind::Review:
            return "review";
        case TaskKind::Other:
            return "other";
    }
    return "other";
}

std::optional<TaskKind> parse_task_kind(std::string_view text) noexcept {
    if (text == "coursework") {
        return TaskKind::CourseWork;
    }
    if (text == "selfstudy") {
        return TaskKind::SelfStudy;
    }
    if (text == "activity") {
        return TaskKind::Activity;
    }
    if (text == "review") {
        return TaskKind::Review;
    }
    if (text == "other") {
        return TaskKind::Other;
    }
    return std::nullopt;
}

std::string_view kind_label(TaskKind kind) noexcept {
    switch (kind) {
        case TaskKind::CourseWork:
            return "课程作业";
        case TaskKind::SelfStudy:
            return "自主学习";
        case TaskKind::Activity:
            return "活动";
        case TaskKind::Review:
            return "复习考试";
        case TaskKind::Other:
            return "其他";
    }
    return "其他";
}

bool is_fixed_time(TaskKind kind) noexcept { return kind == TaskKind::Activity; }

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
