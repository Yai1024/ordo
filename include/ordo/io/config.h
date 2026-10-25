#pragma once

// config.ini 解析（OUTLINE §7.5、ADR-0003）。
// 只负责读取与格式校验；"今天有哪些空闲时段"由 app/plan_service 决定。

#include <filesystem>

#include "ordo/core/result.h"
#include "ordo/core/types.h"

namespace ordo {

// 时间一律用"零点起的分钟数"表示，避免在算法层反复做字符串解析。
struct ScheduleConfig {
    Minutes weekday_start{18 * 60 + 30};
    Minutes weekday_end{23 * 60};
    Minutes weekend_start{9 * 60};
    Minutes weekend_end{22 * 60};
    Minutes break_minutes{10};
    Minutes default_estimate{60};
    bool allow_split{true};
    Minutes min_block{15};
};

// 文件不存在视为使用默认值（与 tasks.csv 的首次运行语义一致）。
// 已出现的键值非法时返回 InvalidFormat，并在消息中带行号。
Result<ScheduleConfig> load_config(const std::filesystem::path& path);

}  // namespace ordo
