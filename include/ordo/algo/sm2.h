#pragma once

// SM-2 间隔重复（ADR-0004）。
// 纯函数：不取时钟、不读文件；"现在几点"由调用方传入。

#include "ordo/core/result.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {

struct Sm2State {
    int stage{0};                  // 连续答对的重复次数
    double ease{kDefaultEase};     // 难度系数 [1.3, 5.0]
    int interval_days{0};          // 当前间隔（天）
};

// quality：0 完全忘记 … 5 毫不费力。非法质量返回 InvalidValue。
Result<Sm2State> sm2_update(const Sm2State& current, int quality);

// 只有复习类任务参与到期判断；从未复习视为立即到期。
bool review_due(const Task& task, TimePoint now);

// 距离下次到期还有几天（向上取整）；已到期返回 0。
int review_days_until_due(const Task& task, TimePoint now);

}  // namespace ordo
