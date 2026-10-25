#pragma once

// C3 算法层：时间块装箱（OUTLINE §7.4、ADR-0003）。
// 纯函数：Tasks 必须已按优先级排好序；不读文件、不取系统时钟、不抛异常。

#include <vector>

#include "ordo/core/schedule.h"
#include "ordo/core/slot.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {

struct PackPolicy {
    bool allow_split{true};  // 是否允许把任务拆成多个块
    Minutes min_block{15};   // 拆分出的块不短于该值
    Minutes gap{10};         // 相邻块之间的休息
};

struct PackResult {
    std::vector<Block> blocks;
    std::vector<Warning> warnings;
};

// 活动按 fixed_start 整块占位，并从空闲时段中扣除；
// 其余任务按传入顺序贪心填充。返回的 blocks 已按开始时间排序。
PackResult pack(const std::vector<Task>& tasks, const std::vector<FreeSlot>& slots,
                const PackPolicy& policy, TimePoint now);

}  // namespace ordo
