// M2：装箱与冲突（OUTLINE §11：恰好装满、超载、零时长、活动占位）。

#include <string>
#include <string_view>
#include <vector>

#include "ordo/algo/conflict.h"
#include "ordo/algo/packer.h"
#include "ordo/core/schedule.h"
#include "ordo/core/slot.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"
#include "test_framework.h"

namespace {

ordo::TimePoint at(std::string_view text) {
    const auto parsed = ordo::parse_datetime(text);
    return parsed.has_value() ? *parsed : ordo::TimePoint{};
}

ordo::Task make_task(ordo::TaskId id, ordo::TaskKind kind, ordo::Minutes estimate,
                     std::string_view deadline) {
    ordo::Task task;
    task.id = id;
    task.title = "task";
    task.kind = kind;
    task.estimate = estimate;
    task.deadline = at(deadline);
    return task;
}

ordo::Task make_activity(ordo::TaskId id, std::string_view start, ordo::Minutes estimate) {
    ordo::Task task = make_task(id, ordo::TaskKind::Activity, estimate, "2026-10-26 23:00");
    task.fixed_start = at(start);
    return task;
}

ordo::FreeSlot slot(std::string_view begin, std::string_view end) {
    return ordo::FreeSlot{at(begin), at(end)};
}

bool has_reason(const std::vector<ordo::Warning>& warnings, std::string_view text) {
    for (const auto& warning : warnings) {
        if (warning.reason.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

const ordo::TimePoint kNow = at("2026-10-26 18:00");

}  // namespace

ORDO_TEST(packer_fills_slot_exactly) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 60, "2026-10-26 20:00");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 19:00")},
                                   policy, kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{1});
    ORDO_CHECK_EQ(ordo::block_minutes(result.blocks[0]), ordo::Minutes{60});
    ORDO_CHECK(result.warnings.empty());
}

ORDO_TEST(packer_reports_overload_with_remaining_minutes) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 120, "2026-10-26 20:00");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 19:00")},
                                   policy, kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{1});
    ORDO_CHECK_EQ(ordo::block_minutes(result.blocks[0]), ordo::Minutes{60});
    ORDO_CHECK(has_reason(result.warnings, "剩余 60 分钟未安排"));
}

ORDO_TEST(packer_zero_estimate_warns_without_block) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task = make_task(1, ordo::TaskKind::SelfStudy, 0, "2026-10-26 20:00");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 19:00")},
                                   policy, kNow);

    ORDO_CHECK(result.blocks.empty());
    ORDO_CHECK(has_reason(result.warnings, "预估时长为 0"));
}

ORDO_TEST(packer_does_not_cross_deadline) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 120, "2026-10-26 18:30");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 19:00")},
                                   policy, kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{1});
    ORDO_CHECK(result.blocks[0].end == at("2026-10-26 18:30"));
    ORDO_CHECK(has_reason(result.warnings, "剩余 90 分钟未安排"));
}

ORDO_TEST(packer_activity_occupies_fixed_time) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task activity = make_activity(1, "2026-10-26 19:00", 60);
    const ordo::Task first =
        make_task(2, ordo::TaskKind::CourseWork, 60, "2026-10-27 23:00");
    const ordo::Task second =
        make_task(3, ordo::TaskKind::SelfStudy, 60, "2026-10-27 23:00");

    const auto result =
        ordo::pack({first, second, activity},
                   {slot("2026-10-26 18:00", "2026-10-26 21:00")}, policy, kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{3});

    bool activity_placed = false;
    for (const ordo::Block& block : result.blocks) {
        if (block.task == 1) {
            ORDO_CHECK(block.begin == at("2026-10-26 19:00"));
            ORDO_CHECK(block.end == at("2026-10-26 20:00"));
            activity_placed = true;
        }
    }
    ORDO_CHECK(activity_placed);

    // 活动时段不能被其它任务占用。
    for (std::size_t i = 0; i < result.blocks.size(); ++i) {
        for (std::size_t j = i + 1; j < result.blocks.size(); ++j) {
            ORDO_CHECK(!ordo::overlaps(result.blocks[i].begin, result.blocks[i].end,
                                       result.blocks[j].begin, result.blocks[j].end));
        }
    }
}

ORDO_TEST(packer_skips_activity_that_already_ended) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task activity = make_activity(1, "2026-10-26 17:00", 30);

    const auto result = ordo::pack({activity},
                                   {slot("2026-10-26 18:00", "2026-10-26 19:00")}, policy, kNow);

    ORDO_CHECK(result.blocks.empty());
    ORDO_CHECK(has_reason(result.warnings, "活动时间已过"));
}

ORDO_TEST(packer_without_split_needs_contiguous_slot) {
    const ordo::PackPolicy policy{false, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 90, "2026-10-26 23:00");

    const auto result =
        ordo::pack({task},
                   {slot("2026-10-26 18:00", "2026-10-26 19:00"),
                    slot("2026-10-26 19:10", "2026-10-26 20:00")},
                   policy, kNow);

    ORDO_CHECK(result.blocks.empty());
    ORDO_CHECK(has_reason(result.warnings, "可用时间不足"));
}

ORDO_TEST(packer_applies_gap_between_blocks) {
    const ordo::PackPolicy policy{true, 15, 10};
    const ordo::Task first =
        make_task(1, ordo::TaskKind::CourseWork, 30, "2026-10-26 23:00");
    const ordo::Task second =
        make_task(2, ordo::TaskKind::CourseWork, 30, "2026-10-26 23:00");

    const auto result =
        ordo::pack({first, second}, {slot("2026-10-26 18:00", "2026-10-26 19:20")}, policy,
                   kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{2});
    ORDO_CHECK(result.blocks[0].end == at("2026-10-26 18:30"));
    ORDO_CHECK(result.blocks[1].begin == at("2026-10-26 18:40"));
}

ORDO_TEST(packer_skips_chunks_below_min_block) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 100, "2026-10-26 23:00");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 18:10")},
                                   policy, kNow);

    ORDO_CHECK(result.blocks.empty());
    ORDO_CHECK(has_reason(result.warnings, "可用时间不足"));
}

ORDO_TEST(packer_schedules_overdue_task_and_warns) {
    const ordo::PackPolicy policy{true, 15, 0};
    const ordo::Task task =
        make_task(1, ordo::TaskKind::CourseWork, 30, "2026-10-26 17:00");

    const auto result = ordo::pack({task}, {slot("2026-10-26 18:00", "2026-10-26 19:00")},
                                   policy, kNow);

    ORDO_CHECK_EQ(result.blocks.size(), std::size_t{1});
    ORDO_CHECK(result.blocks[0].begin == at("2026-10-26 18:00"));
    ORDO_CHECK(has_reason(result.warnings, "已超期"));
}

ORDO_TEST(conflict_detects_overlapping_blocks_for_both_tasks) {
    const std::vector<ordo::Block> blocks{
        ordo::Block{1, at("2026-10-26 18:00"), at("2026-10-26 19:00")},
        ordo::Block{2, at("2026-10-26 18:30"), at("2026-10-26 19:30")},
    };

    const std::vector<ordo::Warning> warnings = ordo::detect_conflicts(blocks);
    ORDO_CHECK_EQ(warnings.size(), std::size_t{2});
    ORDO_CHECK(has_reason(warnings, "重叠"));
}

ORDO_TEST(conflict_ignores_touching_blocks) {
    const std::vector<ordo::Block> blocks{
        ordo::Block{1, at("2026-10-26 18:00"), at("2026-10-26 19:00")},
        ordo::Block{2, at("2026-10-26 19:00"), at("2026-10-26 20:00")},
    };

    ORDO_CHECK(ordo::detect_conflicts(blocks).empty());
}

ORDO_TEST(slot_subtract_leaves_both_sides) {
    const std::vector<ordo::FreeSlot> slots{slot("2026-10-26 18:00", "2026-10-26 22:00")};
    const std::vector<ordo::FreeSlot> remaining =
        ordo::subtract(slots, slot("2026-10-26 19:00", "2026-10-26 20:00"));

    ORDO_CHECK_EQ(remaining.size(), std::size_t{2});
    ORDO_CHECK(remaining[0].begin == at("2026-10-26 18:00"));
    ORDO_CHECK(remaining[0].end == at("2026-10-26 19:00"));
    ORDO_CHECK(remaining[1].begin == at("2026-10-26 20:00"));
    ORDO_CHECK(remaining[1].end == at("2026-10-26 22:00"));
}
