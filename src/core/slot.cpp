#include "ordo/core/slot.h"

#include <algorithm>

namespace ordo {

Minutes slot_minutes(const FreeSlot& slot) noexcept { return minutes_between(slot.begin, slot.end); }

Minutes block_minutes(const Block& block) noexcept {
    return minutes_between(block.begin, block.end);
}

bool is_valid(const FreeSlot& slot) noexcept { return slot.begin < slot.end; }

bool is_valid(const Block& block) noexcept { return block.begin < block.end; }

bool overlaps(TimePoint a_begin, TimePoint a_end, TimePoint b_begin, TimePoint b_end) noexcept {
    return a_begin < b_end && b_begin < a_end;
}

std::optional<FreeSlot> intersect(const FreeSlot& left, const FreeSlot& right) noexcept {
    const TimePoint begin = std::max(left.begin, right.begin);
    const TimePoint end = std::min(left.end, right.end);
    if (begin >= end) {
        return std::nullopt;
    }
    return FreeSlot{begin, end};
}

std::vector<FreeSlot> subtract(const std::vector<FreeSlot>& slots, const FreeSlot& occupied) {
    std::vector<FreeSlot> result;
    result.reserve(slots.size());

    if (!is_valid(occupied)) {
        return slots;
    }

    for (const FreeSlot& slot : slots) {
        if (!overlaps(slot.begin, slot.end, occupied.begin, occupied.end)) {
            result.push_back(slot);
            continue;
        }

        // 被占用区间从左侧切入，保留右侧剩余部分。
        if (slot.begin < occupied.begin) {
            result.push_back(FreeSlot{slot.begin, std::min(slot.end, occupied.begin)});
        }
        // 被占用区间从右侧切入，保留左侧剩余部分。
        if (occupied.end < slot.end) {
            result.push_back(FreeSlot{std::max(slot.begin, occupied.end), slot.end});
        }
    }

    return result;
}

}  // namespace ordo
