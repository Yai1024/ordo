#pragma once

// C3 算法层：时间块冲突检测（OUTLINE §7.4、ADR-0003）。
// 半开区间下，两个块仅共享端点不算冲突。

#include <vector>

#include "ordo/core/schedule.h"
#include "ordo/core/slot.h"

namespace ordo {

// 每个重叠对给双方各生成一条 Warning；无效块（begin >= end）也报告。
std::vector<Warning> detect_conflicts(const std::vector<Block>& blocks);

}  // namespace ordo
