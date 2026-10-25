#include "ordo/ui/app_model.h"

#include <charconv>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

#include "ordo/algo/sm2.h"
#include "ordo/app/plan_service.h"
#include "ordo/core/schedule.h"

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

TaskRow to_row(const Task& task, TimePoint now) {
    TaskRow row;
    row.id = task.id;
    row.title = task.title;
    row.course = task.course;
    row.kind_label = std::string{kind_label(task.kind)};
    row.importance = task.importance;
    row.estimate = task.estimate;
    row.deadline_text = format_datetime(task.deadline);
    row.state = task.state;
    if (task.kind == TaskKind::Review) {
        if (!task.last_review.has_value()) {
            row.review_text = "未复习";
        } else if (review_due(task, now)) {
            row.review_text = "到期";
        } else {
            row.review_text = std::to_string(review_days_until_due(task, now)) + " 天后";
        }
    }
    return row;
}

}  // namespace

AppModel::AppModel(ITaskRepository& repo, std::function<TimePoint()> clock, ScheduleConfig config,
                   IEventLog* log)
    : repo_(repo), clock_(std::move(clock)), config_(config), log_(log) {}

Result<std::vector<TaskRow>> AppModel::list(const ListQuery& query) const {
    TaskService service(repo_, log_);
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
        rows.push_back(to_row(task, clock_()));
    }

    return Result<std::vector<TaskRow>>::ok(std::move(rows));
}

Result<Stats> AppModel::stats() const {
    TaskService service(repo_, log_);
    return service.stats(clock_());
}

Result<TaskForm> AppModel::form_for(TaskId id) const {
    TaskService service(repo_, log_);
    auto found = service.get(id);
    if (!found.has_value()) {
        return forward_failure<TaskForm>(found.status());
    }

    const Task& task = found.value();
    TaskForm form;
    form.title = task.title;
    form.course = task.course;
    form.kind = std::string{to_string(task.kind)};
    form.importance = std::to_string(task.importance);
    form.estimate = std::to_string(task.estimate);
    form.deadline = format_datetime(task.deadline);
    form.start = task.fixed_start.has_value() ? format_datetime(*task.fixed_start) : std::string{};
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

    const std::string kind_text = form.kind.empty() ? std::string{"other"} : form.kind;
    const std::optional<TaskKind> kind = parse_task_kind(trim(kind_text));
    if (!kind.has_value()) {
        return Result<Task>::fail(
            ErrorCode::InvalidFormat,
            "任务类型只能是 coursework/selfstudy/activity/review/other");
    }

    Task task;
    task.id = id;
    task.title = std::string{trim(form.title)};
    task.course = std::string{trim(form.course)};
    task.importance = *importance;
    task.estimate = static_cast<Minutes>(*estimate);
    task.kind = *kind;
    task.state = form.state.value_or(TaskState::Todo);

    if (is_fixed_time(*kind)) {
        if (trim(form.start).empty()) {
            return Result<Task>::fail(ErrorCode::InvalidFormat, "活动类任务必须填写开始时间");
        }
        const std::optional<TimePoint> start = parse_datetime(form.start);
        if (!start.has_value()) {
            return Result<Task>::fail(ErrorCode::InvalidFormat,
                                      "开始时间格式应为 YYYY-MM-DD HH:MM");
        }
        if (*estimate <= 0) {
            return Result<Task>::fail(ErrorCode::InvalidValue,
                                      "活动类任务的预估时长必须大于 0");
        }

        task.fixed_start = *start;
        if (trim(form.deadline).empty()) {
            // 活动默认占满 estimate 分钟，截止时间就是结束时刻。
            task.deadline = add_minutes(*start, static_cast<Minutes>(*estimate));
        } else {
            const std::optional<TimePoint> deadline = parse_datetime(form.deadline);
            if (!deadline.has_value()) {
                return Result<Task>::fail(ErrorCode::InvalidFormat,
                                          "截止时间格式应为 YYYY-MM-DD HH:MM");
            }
            task.deadline = *deadline;
        }
    } else {
        if (!trim(form.start).empty()) {
            return Result<Task>::fail(ErrorCode::InvalidValue,
                                      "只有活动类任务可以填写开始时间");
        }
        const std::optional<TimePoint> deadline = parse_datetime(form.deadline);
        if (!deadline.has_value()) {
            return Result<Task>::fail(ErrorCode::InvalidFormat,
                                      "截止时间格式应为 YYYY-MM-DD HH:MM");
        }
        task.deadline = *deadline;
    }

    return Result<Task>::ok(std::move(task));
}

Result<TaskId> AppModel::create(const TaskForm& form) {
    auto built = build_task(0, form);
    if (!built.has_value()) {
        return forward_failure<TaskId>(built.status());
    }

    TaskService service(repo_, log_);
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

    TaskService service(repo_, log_);
    auto existing = service.get(id);
    if (!existing.has_value()) {
        return existing.status();
    }

    // 表单不编辑复习状态，更新时保留原值（M3 的 SM-2 数据不能因为改标题而丢失）。
    Task task = std::move(built.value());
    task.review_stage = existing.value().review_stage;
    task.ease_factor = existing.value().ease_factor;
    task.interval_days = existing.value().interval_days;
    task.last_review = existing.value().last_review;

    return service.update(id, task);
}

Status AppModel::remove(TaskId id) {
    TaskService service(repo_, log_);
    return service.remove(id);
}

Status AppModel::set_state(TaskId id, TaskState state) {
    TaskService service(repo_, log_);
    return service.set_state(id, state);
}

Result<PlanView> AppModel::plan_today(RankMode rank) const {
    return plan(false, rank);
}

Result<PlanView> AppModel::plan_week(RankMode rank) const {
    return plan(true, rank);
}

Result<PlanView> AppModel::plan(bool week, RankMode rank) const {
    const TimePoint now = clock_();

    PlanService planner(repo_, config_);
    auto planned = week ? planner.plan_week(now, rank) : planner.plan_today(now, rank);
    if (!planned.has_value()) {
        return forward_failure<PlanView>(planned.status());
    }

    TaskService service(repo_, log_);
    auto all = service.list(std::nullopt);
    if (!all.has_value()) {
        return forward_failure<PlanView>(all.status());
    }

    std::unordered_map<TaskId, Task> by_id;
    by_id.reserve(all.value().size());
    for (const Task& task : all.value()) {
        by_id.emplace(task.id, task);
    }

    const Plan& source = planned.value();
    PlanView view;
    view.total_free = source.total_free;
    view.total_assigned = source.total_assigned;
    view.rows.reserve(source.blocks.size());

    for (const Block& block : source.blocks) {
        PlanRow row;
        if (week) {
            row.time_text = format_date(block.begin) + " " + format_time(block.begin) + "-" +
                            format_time(block.end);
        } else {
            row.time_text = format_time(block.begin) + "-" + format_time(block.end);
        }

        const auto found = by_id.find(block.task);
        if (found != by_id.end()) {
            row.title = found->second.title;
            row.kind_label = std::string{kind_label(found->second.kind)};
        } else {
            row.title = "<未知任务>";
            row.kind_label = "-";
        }
        view.rows.push_back(std::move(row));
    }

    view.warnings.reserve(source.warnings.size());
    for (const Warning& warning : source.warnings) {
        std::string text = "#" + std::to_string(warning.task);
        const auto found = by_id.find(warning.task);
        if (found != by_id.end() && !found->second.title.empty()) {
            text += " " + found->second.title;
        }
        text += "：" + warning.reason;
        view.warnings.push_back(std::move(text));
    }

    return Result<PlanView>::ok(std::move(view));
}

Result<WeeklyStatsView> AppModel::weekly_stats() const {
    const TimePoint now = clock_();
    TaskService service(repo_, log_);
    auto stats = service.weekly_stats(now);
    if (!stats.has_value()) {
        return forward_failure<WeeklyStatsView>(stats.status());
    }

    const WeeklyStats& source = stats.value();
    WeeklyStatsView view;
    view.window_text = format_date(source.window_begin) + " ~ " + format_date(source.window_end);
    view.completed = source.completed;
    view.expected = source.expected;
    view.has_rate = source.has_rate;
    view.completion_percent = source.completion_percent;
    view.total_minutes = source.total_minutes;

    view.by_kind.reserve(source.by_kind.size());
    for (const KindMinutes& item : source.by_kind) {
        KindMinutesRow row;
        row.kind_label = std::string{kind_label(item.kind)};
        row.minutes = item.minutes;
        row.percent = source.total_minutes > 0
                          ? static_cast<int>(item.minutes * 100 / source.total_minutes)
                          : 0;
        view.by_kind.push_back(std::move(row));
    }

    return Result<WeeklyStatsView>::ok(std::move(view));
}

Result<TaskRow> AppModel::grade_review(TaskId id, int quality, std::optional<Minutes> spent) {
    const TimePoint now = clock_();
    TaskService service(repo_, log_);
    auto graded = service.grade_review(id, quality, now, spent);
    if (!graded.has_value()) {
        return forward_failure<TaskRow>(graded.status());
    }

    return Result<TaskRow>::ok(to_row(graded.value(), now));
}

Result<std::vector<TaskRow>> AppModel::due_reviews() const {
    const TimePoint now = clock_();
    TaskService service(repo_, log_);
    auto due = service.due_reviews(now);
    if (!due.has_value()) {
        return forward_failure<std::vector<TaskRow>>(due.status());
    }

    std::vector<TaskRow> rows;
    rows.reserve(due.value().size());
    for (const Task& task : due.value()) {
        rows.push_back(to_row(task, now));
    }
    return Result<std::vector<TaskRow>>::ok(std::move(rows));
}

}  // namespace ordo
