#pragma once

// 时间区间（OUTLINE §6、ADR-0003）。
// FreeSlot / Block 都是半开区间 [begin, end)：同一时刻只属于一侧。
// 这一层只做区间运算，不读时钟、不碰文件。

#include <optional>
#include <vector>

#include "ordo/core/types.h"

namespace ordo {

struct FreeSlot {
    TimePoint begin;
    TimePoint end;
};

struct Block {
    TaskId task{};
    TimePoint begin;
    TimePoint end;
};

Minutes slot_minutes(const FreeSlot& slot) noexcept;
Minutes block_minutes(const Block& block) noexcept;

bool is_valid(const FreeSlot& slot) noexcept;
bool is_valid(const Block& block) noexcept;

// 半开区间相交：a.begin < b.end && b.begin < a.end。
bool overlaps(TimePoint a_begin, TimePoint a_end, TimePoint b_begin, TimePoint b_end) noexcept;

std::optional<FreeSlot> intersect(const FreeSlot& left, const FreeSlot& right) noexcept;

// 从 slots 中扣除 occupied，返回剩余区间，保持升序。
std::vector<FreeSlot> subtract(const std::vector<FreeSlot>& slots, const FreeSlot& occupied);

}  // namespace ordo
