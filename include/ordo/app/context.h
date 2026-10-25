#pragma once

// 运行期上下文（OUTLINE §5.2）：命令层唯一能拿到的外部依赖。
// 时钟以函数形式注入，既保证命令可测，又让 algo/ 保持"不取系统时间"。

#include <functional>
#include <istream>
#include <ostream>

#include "ordo/core/types.h"
#include "ordo/io/config.h"
#include "ordo/io/repository.h"

namespace ordo {

struct AppContext {
    ITaskRepository& repo;
    std::ostream& out;
    std::istream& in;
    std::function<TimePoint()> clock;
    ScheduleConfig schedule;

    TimePoint now() const { return clock ? clock() : Clock::now(); }
};

}  // namespace ordo
