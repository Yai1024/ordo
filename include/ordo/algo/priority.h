#pragma once

// C3 算法层：加权打分与排序（OUTLINE §7.4）。
// 纯函数：不读文件、不取系统时钟（now 由参数传入）、不抛异常。

#include <vector>

#include "ordo/core/task.h"
#include "ordo/core/types.h"

namespace ordo {

// 没有配置数据时的每日可投入时长（8 小时），M2 接入 config.ini 后改为可配置。
constexpr Minutes kDefaultDailyCapacity = 8 * kMinutesPerHour;

struct ScoreContext {
    TimePoint now;
    Minutes daily_capacity{kDefaultDailyCapacity};
};

// 打分函数可替换：默认加权，之后可以换成更复杂的模型。
using ScoreFn = double (*)(const Task&, const ScoreContext&);

// 分数范围约 [0, 200]：重要度 50 分 + 紧急度 30 分 + 短任务奖励 20 分，
// 超期任务在紧急度上额外获得最多 30 分。分数越高越应该先做。
double score_weighted(const Task& task, const ScoreContext& context);

// 按分数降序；并列时早截止优先，再并列时 id 小的优先（保证结果确定）。
std::vector<TaskId> rank_by_score(const std::vector<Task>& tasks, ScoreFn score,
                                  const ScoreContext& context);

}  // namespace ordo
