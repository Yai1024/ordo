#pragma once

// 一次编排的结果（OUTLINE §6、ADR-0003）。
// 排不上不是错误，是正常业务结果：未排上的任务放进 warnings 并说明原因。

#include <string>
#include <vector>

#include "ordo/core/slot.h"
#include "ordo/core/types.h"

namespace ordo {

struct Warning {
    TaskId task{};
    std::string reason;
};

struct Plan {
    std::vector<Block> blocks;
    std::vector<Warning> warnings;
    Minutes total_assigned{0};
    Minutes total_free{0};
};

void add_warning(Plan& plan, TaskId task, std::string reason);

// 所有时间块的分钟数之和。
Minutes sum_minutes(const std::vector<Block>& blocks) noexcept;

// 按开始时间升序；同一开始时间按任务 id 升序，保证输出确定。
void sort_blocks(std::vector<Block>& blocks);

}  // namespace ordo
