// M1：应用层业务规则（OUTLINE §11：用假仓库验证拒绝路径）。

#include <chrono>
#include <string>
#include <vector>

#include "ordo/app/task_service.h"
#include "ordo/core/task.h"
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
