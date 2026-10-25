#include "ordo/app/plan_service.h"

#include <algorithm>
#include <ctime>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ordo/algo/conflict.h"
#include "ordo/algo/edf.h"
#include "ordo/algo/packer.h"
#include "ordo/algo/priority.h"
#include "ordo/core/task.h"

namespace ordo {
namespace {

// 计划以分钟为最小粒度：把 now 对齐到整分钟，避免块边界出现 20:59 这类显示偏差。
TimePoint floor_to_minute(TimePoint point) {
    return TimePoint(std::chrono::duration_cast<std::chrono::minutes>(point.time_since_epoch()));
}

std::tm to_local(TimePoint point) {
    const std::time_t seconds = Clock::to_time_t(point);
    std::tm result{};
    if (const std::tm* local = std::localtime(&seconds)) {
        result = *local;
    }
    return result;
}

TimePoint local_midnight(TimePoint point) {
    std::tm parts = to_local(point);
    parts.tm_hour = 0;
    parts.tm_min = 0;
    parts.tm_sec = 0;
    parts.tm_isdst = -1;
    return Clock::from_time_t(std::mktime(&parts));
}

TimePoint next_local_day(TimePoint day) {
    std::tm parts = to_local(day);
    parts.tm_mday += 1;
    parts.tm_isdst = -1;
    return Clock::from_time_t(std::mktime(&parts));
}

bool is_weekend(TimePoint day) {
    const std::tm parts = to_local(day);
    return parts.tm_wday == 0 || parts.tm_wday == 6;  // 周日 / 周六
}

Minutes day_start_minutes(TimePoint day, const ScheduleConfig& config) {
    return is_weekend(day) ? config.weekend_start : config.weekday_start;
}

Minutes day_end_minutes(TimePoint day, const ScheduleConfig& config) {
    return is_weekend(day) ? config.weekend_end : config.weekday_end;
}

std::vector<FreeSlot> build_free_slots(TimePoint now, int days, const ScheduleConfig& config) {
    std::vector<FreeSlot> slots;
    slots.reserve(static_cast<std::size_t>(days));

    TimePoint day = local_midnight(now);
    for (int offset = 0; offset < days; ++offset) {
        TimePoint begin = add_minutes(day, day_start_minutes(day, config));
        const TimePoint end = add_minutes(day, day_end_minutes(day, config));

        if (offset == 0) {
            begin = std::max(begin, now);
        }
        if (begin < end) {
            slots.push_back(FreeSlot{begin, end});
        }
        day = next_local_day(day);
    }

    return slots;
}

TimePoint horizon_end(TimePoint now, int days, const ScheduleConfig& config) {
    TimePoint day = local_midnight(now);
    for (int offset = 1; offset < days; ++offset) {
        day = next_local_day(day);
    }
    return add_minutes(day, day_end_minutes(day, config));
}

Minutes daily_capacity(const ScheduleConfig& config) {
    const Minutes weekday = config.weekday_end - config.weekday_start;
    const Minutes weekend = config.weekend_end - config.weekend_start;
    return std::max<Minutes>({weekday, weekend, 1});
}

}  // namespace

PlanService::PlanService(ITaskRepository& repo, ScheduleConfig config)
    : repo_(repo), config_(config) {}

Result<Plan> PlanService::plan_today(TimePoint now, RankMode rank) const {
    return plan_range(now, 1, false, rank);
}

Result<Plan> PlanService::plan_week(TimePoint now, RankMode rank) const {
    return plan_range(now, 7, true, rank);
}

Result<Plan> PlanService::plan_range(TimePoint now, int days, bool due_only,
                                     RankMode rank) const {
    auto loaded = repo_.load_all();
    if (!loaded.has_value()) {
        return Result<Plan>::fail(loaded.status().code, loaded.status().message);
    }

    const TimePoint at = floor_to_minute(now);
    const TimePoint end = horizon_end(at, days, config_);

    std::vector<Task> activities;
    std::vector<Task> regular;
    activities.reserve(loaded.value().size());
    regular.reserve(loaded.value().size());

    for (const Task& task : loaded.value()) {
        if (!is_open(task.state)) {
            continue;
        }

        if (is_fixed_time(task.kind)) {
            if (!task.fixed_start.has_value()) {
                continue;  // 非法数据；validate 已经会拦，这里只做防御
            }
            // 已结束或落在窗口之外的活动不进入本次计划。
            if (add_minutes(*task.fixed_start, task.estimate) <= at) {
                continue;
            }
            if (*task.fixed_start >= end) {
                continue;
            }
            activities.push_back(task);
        } else {
            // 本周计划只排截止在本周内的任务；今日计划不按截止过滤。
            if (due_only && task.deadline > end) {
                continue;
            }
            regular.push_back(task);
        }
    }

    std::vector<TaskId> order;
    if (rank == RankMode::Edf) {
        order = rank_by_edf(regular, at);
    } else {
        const ScoreContext context{at, daily_capacity(config_)};
        order = rank_by_score(regular, score_weighted, context);
    }

    std::unordered_map<TaskId, Task> by_id;
    by_id.reserve(regular.size());
    for (const Task& task : regular) {
        by_id.emplace(task.id, task);
    }

    // 活动先交给 packer 占位，其余任务严格按排序结果加入。
    std::vector<Task> ordered = activities;
    ordered.reserve(activities.size() + order.size());
    for (const TaskId id : order) {
        const auto found = by_id.find(id);
        if (found != by_id.end()) {
            ordered.push_back(found->second);
        }
    }

    const std::vector<FreeSlot> slots = build_free_slots(at, days, config_);
    const PackPolicy policy{config_.allow_split, config_.min_block, config_.break_minutes};

    PackResult packed = pack(ordered, slots, policy, at);

    Plan plan;
    plan.blocks = std::move(packed.blocks);
    plan.warnings = std::move(packed.warnings);

    plan.total_free = 0;
    for (const FreeSlot& slot : slots) {
        plan.total_free += slot_minutes(slot);
    }
    plan.total_assigned = sum_minutes(plan.blocks);

    std::vector<Warning> conflicts = detect_conflicts(plan.blocks);
    plan.warnings.insert(plan.warnings.end(), conflicts.begin(), conflicts.end());

    sort_blocks(plan.blocks);
    return Result<Plan>::ok(std::move(plan));
}

}  // namespace ordo
