// M3：SM-2 间隔序列与到期边界（OUTLINE §11、ADR-0004）。

#include <cmath>
#include <optional>
#include <string>

#include "ordo/algo/sm2.h"
#include "ordo/core/task.h"
#include "test_framework.h"

namespace {

const ordo::TimePoint kNow = *ordo::parse_datetime("2026-10-25 20:00");
constexpr double kEpsilon = 1e-9;

bool nearly(double left, double right) { return std::fabs(left - right) < kEpsilon; }

ordo::Task make_review(std::optional<ordo::TimePoint> last_review, int stage, int interval,
                       double ease = ordo::kDefaultEase) {
    ordo::Task task;
    task.id = 1;
    task.title = "线性代数复习";
    task.kind = ordo::TaskKind::Review;
    task.deadline = ordo::add_minutes(kNow, 30 * ordo::kMinutesPerDay);
    task.review_stage = stage;
    task.interval_days = interval;
    task.ease_factor = ease;
    task.last_review = last_review;
    return task;
}

}  // namespace

ORDO_TEST(sm2_first_success_schedules_one_day) {
    const auto next = ordo::sm2_update(ordo::Sm2State{}, 5);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK_EQ(next.value().stage, 1);
    ORDO_CHECK_EQ(next.value().interval_days, 1);
    ORDO_CHECK(nearly(next.value().ease, 2.6));  // q=5 奖励 0.1
}

ORDO_TEST(sm2_second_success_schedules_six_days) {
    const auto next = ordo::sm2_update(ordo::Sm2State{1, 2.5, 1}, 4);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK_EQ(next.value().stage, 2);
    ORDO_CHECK_EQ(next.value().interval_days, 6);
    ORDO_CHECK(nearly(next.value().ease, 2.5));  // q=4 不改变难度
}

ORDO_TEST(sm2_third_success_uses_ease_before_update) {
    const auto next = ordo::sm2_update(ordo::Sm2State{2, 2.7, 6}, 5);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK_EQ(next.value().stage, 3);
    ORDO_CHECK_EQ(next.value().interval_days, 16);  // round(6 * 2.7) = 16
    ORDO_CHECK(nearly(next.value().ease, 2.8));
}

ORDO_TEST(sm2_failure_resets_stage) {
    const auto next = ordo::sm2_update(ordo::Sm2State{4, 2.5, 30}, 2);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK_EQ(next.value().stage, 0);
    ORDO_CHECK_EQ(next.value().interval_days, 1);
    ORDO_CHECK(nearly(next.value().ease, 2.18));  // 2.5 + 0.1 - 3 * 0.14
}

ORDO_TEST(sm2_rejects_invalid_quality) {
    const auto negative = ordo::sm2_update(ordo::Sm2State{}, -1);
    const auto too_big = ordo::sm2_update(ordo::Sm2State{}, 6);
    ORDO_CHECK(!negative.has_value());
    ORDO_CHECK(!too_big.has_value());
    ORDO_CHECK(negative.status().code == ordo::ErrorCode::InvalidValue);
}

ORDO_TEST(sm2_clamps_ease_to_floor) {
    const auto next = ordo::sm2_update(ordo::Sm2State{2, 1.4, 6}, 0);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK(nearly(next.value().ease, ordo::kMinEase));
}

ORDO_TEST(sm2_caps_interval_at_one_year) {
    const auto next = ordo::sm2_update(ordo::Sm2State{10, 5.0, 300}, 5);
    ORDO_CHECK(next.has_value());
    ORDO_CHECK_EQ(next.value().interval_days, ordo::kMaxIntervalDays);
}

ORDO_TEST(review_due_when_never_reviewed) {
    const ordo::Task task = make_review(std::nullopt, 0, 0);
    ORDO_CHECK(ordo::review_due(task, kNow));
    ORDO_CHECK_EQ(ordo::review_days_until_due(task, kNow), 0);
}

ORDO_TEST(review_due_after_interval) {
    const ordo::Task waiting =
        make_review(ordo::add_minutes(kNow, -2 * ordo::kMinutesPerDay), 2, 6);
    const ordo::Task due =
        make_review(ordo::add_minutes(kNow, -7 * ordo::kMinutesPerDay), 2, 6);

    ORDO_CHECK(!ordo::review_due(waiting, kNow));
    ORDO_CHECK(ordo::review_due(due, kNow));
    ORDO_CHECK_EQ(ordo::review_days_until_due(waiting, kNow), 4);
    ORDO_CHECK_EQ(ordo::review_days_until_due(due, kNow), 0);
}

ORDO_TEST(review_days_until_due_rounds_up) {
    // 还差 1 分钟到第 6 天：显示 1 天后，而不是 0。
    const ordo::Task task =
        make_review(ordo::add_minutes(kNow, -(6 * ordo::kMinutesPerDay - 1)), 2, 6);
    ORDO_CHECK(!ordo::review_due(task, kNow));
    ORDO_CHECK_EQ(ordo::review_days_until_due(task, kNow), 1);
}

ORDO_TEST(review_due_ignores_non_review_tasks) {
    ordo::Task task = make_review(std::nullopt, 0, 0);
    task.kind = ordo::TaskKind::CourseWork;
    ORDO_CHECK(!ordo::review_due(task, kNow));
}
