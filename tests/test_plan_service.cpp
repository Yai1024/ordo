// M2：编排服务（ADR-0003：空闲时段、活动占位、今日/本周窗口）。

#include <string>
#include <string_view>
#include <vector>

#include "ordo/app/plan_service.h"
#include "ordo/app/task_service.h"
#include "ordo/core/schedule.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"
#include "ordo/io/config.h"
#include "ordo/io/repository.h"
#include "test_framework.h"

namespace {

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

ordo::TimePoint at(std::string_view text) {
    const auto parsed = ordo::parse_datetime(text);
    return parsed.has_value() ? *parsed : ordo::TimePoint{};
}

ordo::Task make_task(ordo::TaskId id, ordo::TaskKind kind, ordo::Minutes estimate,
                     std::string_view deadline) {
    ordo::Task task;
    task.id = id;
    task.title = "task " + std::to_string(id);
    task.kind = kind;
    task.estimate = estimate;
    task.deadline = at(deadline);
    return task;
}

bool has_warning_for(const std::vector<ordo::Warning>& warnings, ordo::TaskId id,
                     std::string_view text) {
    for (const auto& warning : warnings) {
        if (warning.task == id && warning.reason.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

ORDO_TEST(plan_today_builds_window_from_config) {
    FakeRepo repo;
    repo.tasks.push_back(
        make_task(1, ordo::TaskKind::CourseWork, 60, "2026-10-27 23:59"));

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_today(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().total_free, ordo::Minutes{270});  // 18:30-23:00
    ORDO_CHECK_EQ(plan.value().total_assigned, ordo::Minutes{60});
    ORDO_CHECK_EQ(plan.value().blocks.size(), std::size_t{1});
    ORDO_CHECK(plan.value().blocks[0].begin == at("2026-10-26 18:30"));
    ORDO_CHECK(plan.value().blocks[0].end == at("2026-10-26 19:30"));
}

ORDO_TEST(plan_today_after_window_warns_instead_of_failing) {
    FakeRepo repo;
    repo.tasks.push_back(
        make_task(1, ordo::TaskKind::CourseWork, 60, "2026-10-27 23:59"));

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_today(at("2026-10-26 23:30"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().total_free, ordo::Minutes{0});
    ORDO_CHECK(plan.value().blocks.empty());
    ORDO_CHECK_EQ(plan.value().warnings.size(), std::size_t{1});
    ORDO_CHECK(plan.value().warnings[0].task == 1);
}

ORDO_TEST(plan_week_ignores_tasks_beyond_horizon) {
    FakeRepo repo;
    repo.tasks.push_back(
        make_task(1, ordo::TaskKind::CourseWork, 60, "2026-10-28 23:59"));
    repo.tasks.push_back(
        make_task(2, ordo::TaskKind::CourseWork, 60, "2026-11-02 23:59"));

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_week(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().blocks.size(), std::size_t{1});
    ORDO_CHECK(plan.value().blocks[0].task == 1);
    // 周一至周五 270 分钟，周末 780 分钟。
    ORDO_CHECK_EQ(plan.value().total_free, ordo::Minutes{270 * 5 + 780 * 2});
}

ORDO_TEST(plan_today_places_activity_and_avoids_its_slot) {
    FakeRepo repo;
    ordo::Task activity =
        make_task(1, ordo::TaskKind::Activity, 60, "2026-10-26 20:00");
    activity.fixed_start = at("2026-10-26 19:00");
    repo.tasks.push_back(activity);
    repo.tasks.push_back(
        make_task(2, ordo::TaskKind::CourseWork, 120, "2026-10-27 23:59"));

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_today(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().blocks.size(), std::size_t{3});
    ORDO_CHECK_EQ(plan.value().total_assigned, ordo::Minutes{180});

    bool activity_placed = false;
    for (const ordo::Block& block : plan.value().blocks) {
        if (block.task == 1) {
            ORDO_CHECK(block.begin == at("2026-10-26 19:00"));
            ORDO_CHECK(block.end == at("2026-10-26 20:00"));
            activity_placed = true;
        }
    }
    ORDO_CHECK(activity_placed);

    for (std::size_t i = 0; i < plan.value().blocks.size(); ++i) {
        for (std::size_t j = i + 1; j < plan.value().blocks.size(); ++j) {
            ORDO_CHECK(!ordo::overlaps(plan.value().blocks[i].begin,
                                       plan.value().blocks[i].end,
                                       plan.value().blocks[j].begin,
                                       plan.value().blocks[j].end));
        }
    }
}

ORDO_TEST(plan_today_prefers_coursework_over_selfstudy) {
    FakeRepo repo;
    repo.tasks.push_back(
        make_task(1, ordo::TaskKind::SelfStudy, 30, "2026-10-27 23:59"));
    repo.tasks.push_back(
        make_task(2, ordo::TaskKind::CourseWork, 30, "2026-10-27 23:59"));

    ordo::ScheduleConfig config;
    config.weekday_start = 18 * 60 + 30;
    config.weekday_end = 19 * 60;

    const ordo::PlanService service(repo, config);
    const auto plan = service.plan_today(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().blocks.size(), std::size_t{1});
    ORDO_CHECK(plan.value().blocks[0].task == 2);
    ORDO_CHECK(has_warning_for(plan.value().warnings, 1, "可用时间不足"));
}

ORDO_TEST(plan_today_schedules_overdue_and_warns) {
    FakeRepo repo;
    repo.tasks.push_back(
        make_task(1, ordo::TaskKind::CourseWork, 30, "2026-10-26 17:00"));

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_today(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().blocks.size(), std::size_t{1});
    ORDO_CHECK(has_warning_for(plan.value().warnings, 1, "已超期"));
}

ORDO_TEST(plan_today_with_no_tasks_is_empty_success) {
    FakeRepo repo;

    const ordo::PlanService service(repo, ordo::ScheduleConfig{});
    const auto plan = service.plan_today(at("2026-10-26 18:00"));

    ORDO_CHECK(plan.has_value());
    ORDO_CHECK(plan.value().blocks.empty());
    ORDO_CHECK(plan.value().warnings.empty());
    ORDO_CHECK_EQ(plan.value().total_free, ordo::Minutes{270});
}
