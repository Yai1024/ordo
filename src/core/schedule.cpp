#include "ordo/core/schedule.h"

#include <algorithm>
#include <utility>

namespace ordo {

void add_warning(Plan& plan, TaskId task, std::string reason) {
    plan.warnings.push_back(Warning{task, std::move(reason)});
}

Minutes sum_minutes(const std::vector<Block>& blocks) noexcept {
    Minutes total = 0;
    for (const Block& block : blocks) {
        total += block_minutes(block);
    }
    return total;
}

void sort_blocks(std::vector<Block>& blocks) {
    std::stable_sort(blocks.begin(), blocks.end(), [](const Block& left, const Block& right) {
        if (left.begin != right.begin) {
            return left.begin < right.begin;
        }
        return left.task < right.task;
    });
}

}  // namespace ordo
