#pragma once

// 任务相关命令：task add/list/done/edit/remove/stats（OUTLINE §5.2）。

#include "ordo/cli/dispatcher.h"

namespace ordo {

void register_task_commands(Dispatcher& dispatcher);

}  // namespace ordo
