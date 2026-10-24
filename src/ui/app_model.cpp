#include "ordo/ui/app_model.h"

#include <charconv>
#include <string_view>
#include <system_error>
#include <utility>

namespace ordo {
namespace {

std::optional<int> parse_int(std::string_view text) {
    const std::string_view cleaned = trim(text);
    if (cleaned.empty()) {
        return std::nullopt;
    }
    int value = 0;
    const char* first = cleaned.data();
    const char* last = cleaned.data() + cleaned.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc{} || result.ptr != last) {
        return std::nullopt;
    }
    return value;
}

template <typename T>
Result<T> forward_failure(const Status& status) {
    return Result<T>::fail(status.code, status.message);
}

TaskRow to_row(const Task& task) {
    TaskRow row;
    row.id = task.id;
    row.title = task.title;
    row.course = task.course;
    row.importance = task.importance;
    row.estimate = task.estimate;
    row.deadline_text = format_datetime(task.deadline);
    row.state = task.state;
    return row;
}

}  // namespace

AppModel::AppModel(ITaskRepository& repo, std::function<TimePoint()> clock)
    : repo_(repo), clock_(std::move(clock)) {}

Result<std::vector<TaskRow>> AppModel::list(const ListQuery& query) const {
    TaskService service(repo_);
    auto result = service.list_ranked(query.state, query.rank, clock_());
    if (!result.has_value()) {
        return forward_failure<std::vector<TaskRow>>(result.status());
    }

    std::vector<TaskRow> rows;
    rows.reserve(result.value().size());
    for (const Task& task : result.value()) {
        // 指定了具体状态时，"只看未完成"不再生效，避免两个过滤条件互相抵消。
        if (!query.state.has_value() && query.open_only && !is_open(task.state)) {
            continue;
        }
        rows.push_back(to_row(task));
    }

    return Result<std::vector<TaskRow>>::ok(std::move(rows));
}

Result<Stats> AppModel::stats() const {
    TaskService service(repo_);
    return service.stats(clock_());
}

Result<TaskForm> AppModel::form_for(TaskId id) const {
    TaskService service(repo_);
    auto found = service.get(id);
    if (!found.has_value()) {
        return forward_failure<TaskForm>(found.status());
    }

    const Task& task = found.value();
    TaskForm form;
    form.title = task.title;
    form.course = task.course;
    form.importance = std::to_string(task.importance);
    form.estimate = std::to_string(task.estimate);
    form.deadline = format_datetime(task.deadline);
    form.state = task.state;
    return Result<TaskForm>::ok(std::move(form));
}

Result<Task> AppModel::build_task(TaskId id, const TaskForm& form) const {
    const std::optional<int> importance = parse_int(form.importance);
    if (!importance.has_value()) {
        return Result<Task>::fail(ErrorCode::InvalidFormat, "重要度必须是 1-5 的整数");
    }

    const std::optional<int> estimate = parse_int(form.estimate);
    if (!estimate.has_value()) {
        return Result<Task>::fail(ErrorCode::InvalidFormat, "预估时长必须是非负整数（分钟）");
    }

    const std::optional<TimePoint> deadline = parse_datetime(form.deadline);
    if (!deadline.has_value()) {
        return Result<Task>::fail(ErrorCode::InvalidFormat,
                                  "截止时间格式应为 YYYY-MM-DD HH:MM");
    }

    Task task;
    task.id = id;
    task.title = std::string{trim(form.title)};
    task.course = std::string{trim(form.course)};
    task.importance = *importance;
    task.estimate = static_cast<Minutes>(*estimate);
    task.deadline = *deadline;
    task.state = form.state.value_or(TaskState::Todo);
    return Result<Task>::ok(std::move(task));
}

Result<TaskId> AppModel::create(const TaskForm& form) {
    auto built = build_task(0, form);
    if (!built.has_value()) {
        return forward_failure<TaskId>(built.status());
    }

    TaskService service(repo_);
    auto created = service.add(std::move(built.value()));
    if (!created.has_value()) {
        return forward_failure<TaskId>(created.status());
    }

    return Result<TaskId>::ok(created.value().id);
}

Status AppModel::update(TaskId id, const TaskForm& form) {
    auto built = build_task(id, form);
    if (!built.has_value()) {
        return built.status();
    }

    TaskService service(repo_);
    auto existing = service.get(id);
    if (!existing.has_value()) {
        return existing.status();
    }

    // 表单不编辑复习状态，更新时保留原值（M3 的 SM-2 数据不能因为改标题而丢失）。
    Task task = std::move(built.value());
    task.review_stage = existing.value().review_stage;
    task.last_review = existing.value().last_review;

    return service.update(id, task);
}

Status AppModel::remove(TaskId id) {
    TaskService service(repo_);
    return service.remove(id);
}

Status AppModel::set_state(TaskId id, TaskState state) {
    TaskService service(repo_);
    auto existing = service.get(id);
    if (!existing.has_value()) {
        return existing.status();
    }

    Task task = std::move(existing.value());
    task.state = state;
    return service.update(id, task);
}

}  // namespace ordo
