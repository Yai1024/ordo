#pragma once

// C2 应用服务：一次完整编排（OUTLINE §7.5、ADR-0003）。
// 取任务 → 算空闲时段 → 排序 → 装箱 → 冲突检测；不做格式转换、不打印。

#include "ordo/app/task_service.h"
#include "ordo/core/result.h"
#include "ordo/core/schedule.h"
#include "ordo/core/types.h"
#include "ordo/io/config.h"
#include "ordo/io/repository.h"

namespace ordo {

class PlanService {
public:
    PlanService(ITaskRepository& repo, ScheduleConfig config);

    // 今日：所有未完成任务参与，把今天的空闲时间填满。
    Result<Plan> plan_today(TimePoint now, RankMode rank = RankMode::Score) const;

    // 本周：滚动 7 天，只排截止落在窗口内的任务（含已超期）。
    Result<Plan> plan_week(TimePoint now, RankMode rank = RankMode::Score) const;

private:
    Result<Plan> plan_range(TimePoint now, int days, bool due_only, RankMode rank) const;

    ITaskRepository& repo_;
    ScheduleConfig config_;
};

}  // namespace ordo
