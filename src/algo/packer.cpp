#include "ordo/algo/packer.h"

#include <algorithm>
#include <string>
#include <utility>

namespace ordo {
namespace {

std::string minutes_text(Minutes value) { return std::to_string(value) + " 分钟"; }

void add_warning_all(PackResult& result, TaskId task, std::string reason) {
    result.warnings.push_back(Warning{task, std::move(reason)});
}

// 把 fixed_start 已经过去、但尚未结束的活动裁到 now，避免计划里出现历史块。
Block ongoing_activity_block(const Task& task, TimePoint now) {
    const TimePoint end = add_minutes(*task.fixed_start, task.estimate);
    return Block{task.id, now, end};
}

}  // namespace

PackResult pack(const std::vector<Task>& tasks, const std::vector<FreeSlot>& slots,
                const PackPolicy& policy, TimePoint now) {
    PackResult result;

    // 只保留 now 之后的有效区间；调用方传进来的时段可能从 today 00:00 开始。
    std::vector<FreeSlot> remaining_slots;
    remaining_slots.reserve(slots.size());
    for (const FreeSlot& slot : slots) {
        const TimePoint begin = std::max(slot.begin, now);
        if (begin < slot.end) {
            remaining_slots.push_back(FreeSlot{begin, slot.end});
        }
    }

    // 第一轮：活动是固定承诺，先占位，再从可分配时段里扣掉。
    for (const Task& task : tasks) {
        if (!is_open(task.state) || !is_fixed_time(task.kind)) {
            continue;
        }
        if (!task.fixed_start.has_value()) {
            add_warning_all(result, task.id, "活动缺少固定开始时间");
            continue;
        }

        const TimePoint end = add_minutes(*task.fixed_start, task.estimate);
        if (end <= now) {
            add_warning_all(result, task.id, "活动时间已过，未排入");
            continue;
        }

        Block block;
        if (*task.fixed_start < now) {
            block = ongoing_activity_block(task, now);
            add_warning_all(result, task.id, "活动已开始，剩余时段占位");
        } else {
            block = Block{task.id, *task.fixed_start, end};
        }

        result.blocks.push_back(block);
        remaining_slots = subtract(remaining_slots, FreeSlot{block.begin, block.end});
    }

    // 第二轮：普通任务按优先级顺序贪心填充。
    for (const Task& task : tasks) {
        if (!is_open(task.state) || is_fixed_time(task.kind)) {
            continue;
        }
        if (task.estimate <= 0) {
            add_warning_all(result, task.id, "预估时长为 0，无需安排");
            continue;
        }

        const bool overdue = task.deadline <= now;
        Minutes remaining = task.estimate;
        Minutes assigned = 0;

        for (FreeSlot& slot : remaining_slots) {
            if (remaining <= 0) {
                break;
            }
            if (!is_valid(slot)) {
                continue;
            }

            // 已超期任务不再受截止时间约束，按最早可用时段安排。
            TimePoint limit = slot.end;
            if (!overdue) {
                limit = std::min(limit, task.deadline);
            }
            if (limit <= slot.begin) {
                continue;
            }

            const Minutes available = minutes_between(slot.begin, limit);
            if (available <= 0) {
                continue;
            }

            // 能一次装下就整块安排，此时不受 min_block 限制（短任务也要能排）。
            if (available >= remaining) {
                const TimePoint end = add_minutes(slot.begin, remaining);
                result.blocks.push_back(Block{task.id, slot.begin, end});
                assigned += remaining;
                remaining = 0;
                slot.begin = std::min(slot.end, add_minutes(end, policy.gap));
                break;
            }

            if (!policy.allow_split || available < policy.min_block) {
                continue;
            }

            const TimePoint end = add_minutes(slot.begin, available);
            result.blocks.push_back(Block{task.id, slot.begin, end});
            assigned += available;
            remaining -= available;
            slot.begin = std::min(slot.end, add_minutes(end, policy.gap));
        }

        if (remaining <= 0) {
            if (overdue) {
                add_warning_all(result, task.id, "已超期，已按最早可用时段安排");
            }
            continue;
        }

        if (assigned == 0) {
            add_warning_all(result, task.id,
                            overdue ? "已超期且没有可用时段"
                                    : "可用时间不足，需要 " + minutes_text(task.estimate));
        } else {
            add_warning_all(result, task.id,
                            "已安排 " + minutes_text(assigned) + "，剩余 " +
                                std::to_string(remaining) + " 分钟未安排");
        }
    }

    sort_blocks(result.blocks);
    return result;
}

}  // namespace ordo
