#pragma once

// 编排命令：plan today / plan week（OUTLINE §5.2、§7.6）。

#include "ordo/cli/dispatcher.h"

namespace ordo {

void register_plan_commands(Dispatcher& dispatcher);

}  // namespace ordo
