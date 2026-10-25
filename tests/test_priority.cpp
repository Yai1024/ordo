// M1：加权排序与 EDF 的边界（OUTLINE §11：每个算法至少 4 个用例，含边界）。

#include <chrono>
#include <cmath>
#include <vector>

#include "ordo/algo/edf.h"
#include "ordo/algo/priority.h"
#include "test_framework.h"

namespace {

const ordo::TimePoint kNow{std::chrono::hours{24 * 365}};

ordo::Task make_task(ordo::TaskId id, int importance, ordo::Minutes estimate,
                     ordo::Minutes until_deadline) {
    ordo::Task task;
    task.id = id;
    task.title = "task";
    task.importance = importance;
    task.estimate = estimate;
    task.deadline = ordo::add_minutes(kNow, until_deadline);
    return task;
}

}  // namespace

ORDO_TEST(score_prefers_higher_importance_when_deadlines_match) {
    const std::vector<ordo::Task> tasks{
        make_task(1, 1, 60, 1000),
        make_task(2, 5, 60, 1000),
    };
    const ordo::ScoreContext context{kNow, ordo::kDefaultDailyCapacity};
    const std::vector<ordo::TaskId> ranked =
        ordo::rank_by_score(tasks, ordo::score_weighted, context);
    ORDO_CHECK_EQ(ranked.size(), std::size_t{2});
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{2});
    ORDO_CHECK_EQ(ranked[1], ordo::TaskId{1});
}

ORDO_TEST(score_prefers_earlier_deadline_when_importance_matches) {
    const std::vector<ordo::Task> tasks{
        make_task(1, 3, 60, 10000),
        make_task(2, 3, 60, 100),
    };
    const ordo::ScoreContext context{kNow, ordo::kDefaultDailyCapacity};
    const std::vector<ordo::TaskId> ranked =
        ordo::rank_by_score(tasks, ordo::score_weighted, context);
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{2});
}

ORDO_TEST(score_puts_overdue_tasks_first) {
    const std::vector<ordo::Task> tasks{
        make_task(1, 3, 60, 10000),   // 尚未超期
        make_task(2, 3, 60, -10000),  // 已超期
    };
    const ordo::ScoreContext context{kNow, ordo::kDefaultDailyCapacity};
    const std::vector<ordo::TaskId> ranked =
        ordo::rank_by_score(tasks, ordo::score_weighted, context);
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{2});
}

ORDO_TEST(score_ties_are_deterministic_by_id) {
    const std::vector<ordo::Task> tasks{
        make_task(3, 3, 60, 500),
        make_task(1, 3, 60, 500),
        make_task(2, 3, 60, 500),
    };
    const ordo::ScoreContext context{kNow, ordo::kDefaultDailyCapacity};
    const std::vector<ordo::TaskId> ranked =
        ordo::rank_by_score(tasks, ordo::score_weighted, context);
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{1});
    ORDO_CHECK_EQ(ranked[1], ordo::TaskId{2});
    ORDO_CHECK_EQ(ranked[2], ordo::TaskId{3});
}

ORDO_TEST(score_prefers_coursework_over_selfstudy_on_ties) {
    ordo::Task coursework = make_task(1, 3, 60, 1000);
    coursework.kind = ordo::TaskKind::CourseWork;
    ordo::Task selfstudy = make_task(2, 3, 60, 1000);
    selfstudy.kind = ordo::TaskKind::SelfStudy;

    const std::vector<ordo::Task> tasks{coursework, selfstudy};
    const ordo::ScoreContext context{kNow, ordo::kDefaultDailyCapacity};
    const std::vector<ordo::TaskId> ranked =
        ordo::rank_by_score(tasks, ordo::score_weighted, context);

    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{1});
    ORDO_CHECK_EQ(ranked[1], ordo::TaskId{2});
}

ORDO_TEST(score_handles_empty_list_and_zero_capacity) {
    const ordo::ScoreContext context{kNow, 0};
    ORDO_CHECK(ordo::rank_by_score({}, ordo::score_weighted, context).empty());

    const ordo::Task zero_estimate = make_task(1, 3, 0, 0);
    const double value = ordo::score_weighted(zero_estimate, context);
    ORDO_CHECK(std::isfinite(value));
    ORDO_CHECK(value >= 0.0);
}

ORDO_TEST(edf_orders_by_deadline) {
    const std::vector<ordo::Task> tasks{
        make_task(1, 3, 60, 3 * ordo::kMinutesPerDay),
        make_task(2, 3, 60, 1 * ordo::kMinutesPerDay),
        make_task(3, 3, 60, 2 * ordo::kMinutesPerDay),
    };
    const std::vector<ordo::TaskId> ranked = ordo::rank_by_edf(tasks, kNow);
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{2});
    ORDO_CHECK_EQ(ranked[1], ordo::TaskId{3});
    ORDO_CHECK_EQ(ranked[2], ordo::TaskId{1});
}

ORDO_TEST(edf_ties_prefer_importance_then_id) {
    const std::vector<ordo::Task> tasks{
        make_task(2, 3, 60, 500),
        make_task(1, 5, 60, 500),
        make_task(3, 3, 60, 500),
    };
    const std::vector<ordo::TaskId> ranked = ordo::rank_by_edf(tasks, kNow);
    ORDO_CHECK_EQ(ranked[0], ordo::TaskId{1});  // 重要度最高
    ORDO_CHECK_EQ(ranked[1], ordo::TaskId{2});  // 其次按 id
    ORDO_CHECK_EQ(ranked[2], ordo::TaskId{3});
}
