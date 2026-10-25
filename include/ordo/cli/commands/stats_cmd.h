#pragma once

// 统计命令：stats week（OUTLINE §5.2、ADR-0004）。

#include "ordo/cli/dispatcher.h"

namespace ordo {

void register_stats_commands(Dispatcher& dispatcher);

}  // namespace ordo
