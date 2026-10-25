// M1：应用层业务规则（OUTLINE §11：用假仓库验证拒绝路径）。

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "ordo/app/task_service.h"
#include "ordo/core/task.h"
#include "ordo/io/event_log.h"
#include "test_framework.h"

namespace {

const ordo::TimePoint kNow{std::chrono::hours{24 * 365}};

class FakeRepo final : public ordo::ITaskRepository {
public:
    std::vector<ordo::Task> tasks;

    ordo::Result<std::vector<ordo::Task>> load_all() const override {
        return ordo::Result<std::vector<ordo::Task>>::ok(tasks);
    }

    ordo::Status save_all(const std::vector<ordo::Task>& value) override {
        tasks = value;
        return ordo::Status::success();
    }

    ordo::Result<ordo::Task> find(ordo::TaskId id) const override {
        for (const ordo::Task& task : tasks) {
            if (task.id == id) {
                return ordo::Result<ordo::Task>::ok(task);
            }
        }
        return ordo::Result<ordo::Task>::fail(ordo::ErrorCode::NotFound, "找不到任务");
    }

    ordo::Status upsert(const ordo::Task& task) override {
        for (ordo::Task& existing : tasks) {
            if (existing.id == task.id) {
                existing = task;
                return ordo::Status::success();
            }
        }
        tasks.push_back(task);
        return ordo::Status::success();
    }

    ordo::Status erase(ordo::TaskId id) override {
        for (auto it = tasks.begin(); it != tasks.end(); ++it) {
            if (it->id == id) {
                tasks.erase(it);
                return ordo::Status::success();
            }
        }
        return ordo::Status::fail(ordo::ErrorCode::NotFound, "找不到任务");
    }
};

ordo::Task make_draft(const std::string& title, ordo::Minutes estimate = 60,
                      ordo::Minutes until_deadline = ordo::kMinutesPerDay) {
    ordo::Task task;
    task.title = title;
    task.estimate = estimate;
    task.deadline = ordo::add_minutes(kNow, until_deadline);
    return task;
}

class FakeLog final : public ordo::IEventLog {
public:
    std::vector<ordo::LogEntry> entries;

    ordo::Status append(const ordo::LogEntry& entry) override {
        entries.push_back(entry);
        return ordo::Status::success();
    }

    ordo::Result<std::vector<ordo::LogEntry>> read() const override {
        return ordo::Result<std::vector<ordo::LogEntry>>::ok(entries);
    }
};

ordo::Task make_review_task(ordo::TaskId id, std::optional<ordo::TimePoint> last_review = {},
                            int stage = 0, int interval_days = 0,
                            ordo::TaskState state = ordo::TaskState::Todo) {
    ordo::Task task = make_draft("复习 " + std::to_string(id));
    task.id = id;
    task.kind = ordo::TaskKind::Review;
    task.review_stage = stage;
    task.interval_days = interval_days;
    task.last_review = last_review;
    task.state = state;
    return task;
}

}  // namespace

ORDO_TEST(add_assigns_sequential_ids) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    const auto first = service.add(make_draft("第一件"));
    const auto second = service.add(make_draft("第二件"));

    ORDO_CHECK(first.has_value());
    ORDO_CHECK(second.has_value());
    ORDO_CHECK_EQ(first.value().id, ordo::TaskId{1});
    ORDO_CHECK_EQ(second.value().id, ordo::TaskId{2});
    ORDO_CHECK_EQ(repo.tasks.size(), std::size_t{2});
}

ORDO_TEST(add_rejects_empty_title) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    const auto result = service.add(make_draft("   "));
    ORDO_CHECK(!result.has_value());
    ORDO_CHECK(result.status().code == ordo::ErrorCode::EmptyInput);
    ORDO_CHECK(repo.tasks.empty());  // 失败不得落盘
}

ORDO_TEST(add_rejects_duplicate_id) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    ordo::Task first = make_draft("第一件");
    first.id = 7;
    ORDO_CHECK(service.add(first).has_value());

    ordo::Task duplicate = make_draft("重复 id");
    duplicate.id = 7;
    const auto result = service.add(duplicate);

    ORDO_CHECK(!result.has_value());
    ORDO_CHECK(result.status().code == ordo::ErrorCode::Duplicate);
    ORDO_CHECK_EQ(repo.tasks.size(), std::size_t{1});
}

ORDO_TEST(update_replaces_fields_and_rejects_unknown_id) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    const auto added = service.add(make_draft("旧标题"));
    ORDO_CHECK(added.has_value());

    ordo::Task patch = added.value();
    patch.title = "新标题";
    patch.importance = 5;
    const ordo::Status updated = service.update(patch.id, patch);
    ORDO_CHECK(updated.ok());

    const auto reloaded = service.get(patch.id);
    ORDO_CHECK(reloaded.has_value());
    ORDO_CHECK_EQ(reloaded.value().title, std::string{"新标题"});
    ORDO_CHECK_EQ(reloaded.value().importance, 5);

    ordo::Task missing_patch = patch;
    missing_patch.id = 999;
    const ordo::Status missing = service.update(999, missing_patch);
    ORDO_CHECK(!missing.ok());
    ORDO_CHECK(missing.code == ordo::ErrorCode::NotFound);
}

ORDO_TEST(remove_rejects_unknown_id) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    const ordo::Status missing = service.remove(42);
    ORDO_CHECK(!missing.ok());
    ORDO_CHECK(missing.code == ordo::ErrorCode::NotFound);
}

ORDO_TEST(list_filters_by_state) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    const auto first = service.add(make_draft("完成项"));
    ORDO_CHECK(first.has_value());
    ORDO_CHECK(service.add(make_draft("待办项")).has_value());

    ordo::Task done = first.value();
    done.state = ordo::TaskState::Done;
    ORDO_CHECK(service.update(done.id, done).ok());

    const auto done_list = service.list(ordo::TaskState::Done);
    ORDO_CHECK(done_list.has_value());
    ORDO_CHECK_EQ(done_list.value().size(), std::size_t{1});

    const auto all = service.list(std::nullopt);
    ORDO_CHECK(all.has_value());
    ORDO_CHECK_EQ(all.value().size(), std::size_t{2});
}

ORDO_TEST(list_ranked_returns_best_first) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    ordo::Task relaxed = make_draft("宽松任务", 60, 30 * ordo::kMinutesPerDay);
    relaxed.importance = 1;
    ORDO_CHECK(service.add(relaxed).has_value());

    ordo::Task urgent = make_draft("紧急任务", 30, 120);
    urgent.importance = 5;
    ORDO_CHECK(service.add(urgent).has_value());

    const auto ranked =
        service.list_ranked(std::nullopt, ordo::RankMode::Score, kNow);
    ORDO_CHECK(ranked.has_value());
    ORDO_CHECK_EQ(ranked.value().size(), std::size_t{2});
    ORDO_CHECK_EQ(ranked.value()[0].title, std::string{"紧急任务"});

    const auto by_edf = service.list_ranked(std::nullopt, ordo::RankMode::Edf, kNow);
    ORDO_CHECK(by_edf.has_value());
    ORDO_CHECK_EQ(by_edf.value()[0].title, std::string{"紧急任务"});
}

ORDO_TEST(stats_counts_overdue_and_estimates) {
    FakeRepo repo;
    ordo::TaskService service(repo);

    ordo::Task overdue = make_draft("超期任务", 90, -60);
    ORDO_CHECK(service.add(overdue).has_value());

    ordo::Task future = make_draft("未来任务", 30, 600);
    future.state = ordo::TaskState::Doing;
    ORDO_CHECK(service.add(future).has_value());

    const auto added_done = service.add(make_draft("已完成", 45));
    ORDO_CHECK(added_done.has_value());
    ordo::Task done = added_done.value();
    done.state = ordo::TaskState::Done;
    ORDO_CHECK(service.update(done.id, done).ok());

    const auto stats = service.stats(kNow);
    ORDO_CHECK(stats.has_value());
    ORDO_CHECK_EQ(stats.value().total, std::size_t{3});
    ORDO_CHECK_EQ(stats.value().done, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().doing, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().overdue, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().open_estimate, ordo::Minutes{120});
    ORDO_CHECK_EQ(stats.value().done_estimate, ordo::Minutes{45});
}

ORDO_TEST(service_logs_write_actions) {
    FakeRepo repo;
    FakeLog log;
    ordo::TaskService service(repo, &log);

    const auto added = service.add(make_draft("第一件"));
    ORDO_CHECK(added.has_value());
    ORDO_CHECK_EQ(log.entries.size(), std::size_t{1});
    ORDO_CHECK_EQ(log.entries[0].command, std::string{"task.add"});
    ORDO_CHECK_EQ(log.entries[0].note, std::string{"第一件"});

    ordo::Task edited = added.value();
    edited.title = "改过的标题";
    ORDO_CHECK(service.update(edited.id, edited).ok());
    ORDO_CHECK_EQ(log.entries.back().command, std::string{"task.edit"});

    ORDO_CHECK(service.set_state(edited.id, ordo::TaskState::Done, 30).ok());
    ORDO_CHECK_EQ(log.entries.back().command, std::string{"task.done"});
    ORDO_CHECK(log.entries.back().minutes.has_value());
    ORDO_CHECK_EQ(*log.entries.back().minutes, ordo::Minutes{30});

    ORDO_CHECK(service.remove(edited.id).ok());
    ORDO_CHECK_EQ(log.entries.back().command, std::string{"task.remove"});
    ORDO_CHECK_EQ(log.entries.back().note, std::string{"改过的标题"});
    ORDO_CHECK_EQ(log.entries.size(), std::size_t{4});
}

ORDO_TEST(service_set_state_without_change_does_not_log) {
    FakeRepo repo;
    FakeLog log;
    ordo::TaskService service(repo, &log);

    const auto added = service.add(make_draft("待办"));
    ORDO_CHECK(added.has_value());
    ORDO_CHECK(service.set_state(added.value().id, ordo::TaskState::Todo).ok());
    ORDO_CHECK_EQ(log.entries.size(), std::size_t{1});
}

ORDO_TEST(service_grade_review_updates_sm2_and_reopens) {
    FakeRepo repo;
    FakeLog log;
    repo.tasks.push_back(make_review_task(1, std::nullopt, 0, 0, ordo::TaskState::Done));

    ordo::TaskService service(repo, &log);
    const auto graded = service.grade_review(1, 4, kNow, 25);
    ORDO_CHECK(graded.has_value());
    ORDO_CHECK_EQ(graded.value().review_stage, 1);
    ORDO_CHECK_EQ(graded.value().interval_days, 1);
    ORDO_CHECK_EQ(graded.value().ease_factor, 2.5);
    ORDO_CHECK(graded.value().state == ordo::TaskState::Todo);
    ORDO_CHECK(graded.value().last_review.has_value());
    ORDO_CHECK_EQ(ordo::format_datetime(*graded.value().last_review),
                  ordo::format_datetime(kNow));

    ORDO_CHECK_EQ(log.entries.size(), std::size_t{1});
    ORDO_CHECK_EQ(log.entries[0].command, std::string{"task.review"});
    ORDO_CHECK(log.entries[0].minutes.has_value());
    ORDO_CHECK_EQ(*log.entries[0].minutes, ordo::Minutes{25});
    ORDO_CHECK_EQ(log.entries[0].note, std::string{"quality=4"});
}

ORDO_TEST(service_grade_review_rejects_non_review) {
    FakeRepo repo;
    FakeLog log;
    repo.tasks.push_back(make_draft("作业"));
    repo.tasks[0].id = 1;

    ordo::TaskService service(repo, &log);
    const auto graded = service.grade_review(1, 5, kNow);
    ORDO_CHECK(!graded.has_value());
    ORDO_CHECK(graded.status().code == ordo::ErrorCode::InvalidValue);
    ORDO_CHECK(log.entries.empty());
}

ORDO_TEST(service_due_reviews_filters_and_sorts) {
    FakeRepo repo;
    repo.tasks.push_back(make_review_task(1));  // 从未复习，最先
    repo.tasks.push_back(
        make_review_task(2, ordo::add_minutes(kNow, -7 * ordo::kMinutesPerDay), 2, 6));
    repo.tasks.push_back(
        make_review_task(3, ordo::add_minutes(kNow, -2 * ordo::kMinutesPerDay), 2, 6));
    repo.tasks.push_back(make_review_task(4, std::nullopt, 0, 0, ordo::TaskState::Done));
    repo.tasks.push_back(make_draft("普通作业"));
    repo.tasks.back().id = 5;

    ordo::TaskService service(repo);
    const auto due = service.due_reviews(kNow);
    ORDO_CHECK(due.has_value());
    ORDO_CHECK_EQ(due.value().size(), std::size_t{2});
    ORDO_CHECK_EQ(due.value()[0].id, ordo::TaskId{1});
    ORDO_CHECK_EQ(due.value()[1].id, ordo::TaskId{2});
}

ORDO_TEST(service_weekly_stats_counts_completed_and_minutes) {
    FakeRepo repo;
    FakeLog log;
    repo.tasks.push_back(make_draft("已完成"));  // id 1
    repo.tasks.back().id = 1;
    repo.tasks.back().state = ordo::TaskState::Done;
    repo.tasks.push_back(make_review_task(2, std::nullopt, 0, 0));
    repo.tasks.back().deadline = ordo::add_minutes(kNow, 30 * ordo::kMinutesPerDay);
    repo.tasks.push_back(make_draft("窗口内到期"));  // id 3
    repo.tasks.back().id = 3;
    repo.tasks.back().deadline = ordo::add_minutes(kNow, -60);
    repo.tasks.push_back(make_draft("旧完成"));  // id 4
    repo.tasks.back().id = 4;
    repo.tasks.back().state = ordo::TaskState::Done;

    ordo::LogEntry done;
    done.timestamp = ordo::add_minutes(kNow, -120);
    done.command = "task.done";
    done.task_id = ordo::TaskId{1};
    log.entries.push_back(done);

    ordo::LogEntry reviewed;
    reviewed.timestamp = ordo::add_minutes(kNow, -60);
    reviewed.command = "task.review";
    reviewed.task_id = ordo::TaskId{2};
    reviewed.minutes = ordo::Minutes{30};
    log.entries.push_back(reviewed);

    ordo::LogEntry old_done;
    old_done.timestamp = ordo::add_minutes(kNow, -10 * ordo::kMinutesPerDay);
    old_done.command = "task.done";
    old_done.task_id = ordo::TaskId{4};
    log.entries.push_back(old_done);

    ordo::TaskService service(repo, &log);
    const auto stats = service.weekly_stats(kNow);
    ORDO_CHECK(stats.has_value());
    ORDO_CHECK_EQ(stats.value().completed, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().expected, std::size_t{2});
    ORDO_CHECK(stats.value().has_rate);
    ORDO_CHECK_EQ(stats.value().completion_percent, 50);
    ORDO_CHECK_EQ(stats.value().total_minutes, ordo::Minutes{30});
    ORDO_CHECK_EQ(stats.value().by_kind.size(), std::size_t{1});
    ORDO_CHECK(stats.value().by_kind[0].kind == ordo::TaskKind::Review);
    ORDO_CHECK_EQ(stats.value().by_kind[0].minutes, ordo::Minutes{30});
}

ORDO_TEST(service_weekly_stats_without_log_uses_open_deadlines) {
    FakeRepo repo;
    repo.tasks.push_back(make_draft("窗口内到期"));
    repo.tasks.back().id = 1;
    repo.tasks.back().deadline = ordo::add_minutes(kNow, -60);
    repo.tasks.push_back(make_draft("未来"));
    repo.tasks.back().id = 2;
    repo.tasks.back().deadline = ordo::add_minutes(kNow, 30 * ordo::kMinutesPerDay);

    ordo::TaskService service(repo);
    const auto stats = service.weekly_stats(kNow);
    ORDO_CHECK(stats.has_value());
    ORDO_CHECK_EQ(stats.value().completed, std::size_t{0});
    ORDO_CHECK_EQ(stats.value().expected, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().completion_percent, 0);
    ORDO_CHECK_EQ(stats.value().total_minutes, ordo::Minutes{0});
}
