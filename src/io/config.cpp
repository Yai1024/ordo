#include "ordo/io/config.h"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace ordo {
namespace {

std::string lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            result.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

bool parse_int_value(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last;
}

// "HH:MM" / "H:MM" → 零点起的分钟数。
bool parse_clock(std::string_view text, Minutes& out) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos || colon == 0 || colon + 3 != text.size()) {
        return false;
    }

    int hour = 0;
    int minute = 0;
    if (!parse_int_value(text.substr(0, colon), hour) ||
        !parse_int_value(text.substr(colon + 1), minute)) {
        return false;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    out = static_cast<Minutes>(hour * 60 + minute);
    return true;
}

bool parse_bool(std::string_view text, bool& out) {
    const std::string value = lower(text);
    if (value == "true" || value == "yes" || value == "1" || value == "on") {
        out = true;
        return true;
    }
    if (value == "false" || value == "no" || value == "0" || value == "off") {
        out = false;
        return true;
    }
    return false;
}

std::string line_prefix(std::size_t line_number) {
    return "第 " + std::to_string(line_number) + " 行: ";
}

Result<ScheduleConfig> fail_config(std::size_t line_number, std::string message) {
    return Result<ScheduleConfig>::fail(ErrorCode::InvalidFormat,
                                        line_prefix(line_number) + std::move(message));
}

}  // namespace

Result<ScheduleConfig> load_config(const std::filesystem::path& path) {
    ScheduleConfig config;

    std::error_code exists_error;
    if (!std::filesystem::exists(path, exists_error)) {
        return Result<ScheduleConfig>::ok(config);
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<ScheduleConfig>::fail(ErrorCode::IoError,
                                            "无法打开配置文件: " + path.string());
    }

    std::string section;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string_view view{line};
        if (line_number == 1) {
            // 容忍 UTF-8 BOM（与 CSV 读取一致）。
            if (view.size() >= 3 && static_cast<unsigned char>(view[0]) == 0xEF &&
                static_cast<unsigned char>(view[1]) == 0xBB &&
                static_cast<unsigned char>(view[2]) == 0xBF) {
                view.remove_prefix(3);
            }
        }

        view = trim(view);
        if (view.empty() || view.front() == '#' || view.front() == ';') {
            continue;
        }

        if (view.front() == '[') {
            if (view.back() != ']') {
                return fail_config(line_number, "节名缺少右方括号");
            }
            section = lower(trim(view.substr(1, view.size() - 2)));
            continue;
        }

        const std::size_t equals = view.find('=');
        if (equals == std::string_view::npos) {
            return fail_config(line_number, "缺少等号");
        }

        const std::string_view key = trim(view.substr(0, equals));
        const std::string_view value = trim(view.substr(equals + 1));

        if (section == "schedule") {
            Minutes parsed = 0;
            if (key == "weekday_start" || key == "weekday_end" || key == "weekend_start" ||
                key == "weekend_end") {
                if (!parse_clock(value, parsed)) {
                    return fail_config(line_number, "时间格式应为 HH:MM: " + std::string{value});
                }
                if (key == "weekday_start") {
                    config.weekday_start = parsed;
                } else if (key == "weekday_end") {
                    config.weekday_end = parsed;
                } else if (key == "weekend_start") {
                    config.weekend_start = parsed;
                } else {
                    config.weekend_end = parsed;
                }
                continue;
            }
            if (key == "break_minutes") {
                int minutes = 0;
                if (!parse_int_value(value, minutes) || minutes < 0) {
                    return fail_config(line_number, "break_minutes 必须是非负整数");
                }
                config.break_minutes = static_cast<Minutes>(minutes);
                continue;
            }
        } else if (section == "policy") {
            if (key == "default_estimate") {
                int minutes = 0;
                if (!parse_int_value(value, minutes) || minutes < 0) {
                    return fail_config(line_number, "default_estimate 必须是非负整数");
                }
                config.default_estimate = static_cast<Minutes>(minutes);
                continue;
            }
            if (key == "min_block") {
                int minutes = 0;
                if (!parse_int_value(value, minutes) || minutes < 0) {
                    return fail_config(line_number, "min_block 必须是非负整数");
                }
                config.min_block = static_cast<Minutes>(minutes);
                continue;
            }
            if (key == "allow_split") {
                bool allow = true;
                if (!parse_bool(value, allow)) {
                    return fail_config(line_number, "allow_split 必须是 true/false");
                }
                config.allow_split = allow;
                continue;
            }
        }

        // 未知的节或键忽略：便于以后追加配置而不破坏旧文件。
    }

    if (input.bad()) {
        return Result<ScheduleConfig>::fail(ErrorCode::IoError,
                                            "读取配置文件失败: " + path.string());
    }

    if (config.weekday_start >= config.weekday_end) {
        return Result<ScheduleConfig>::fail(ErrorCode::InvalidValue,
                                            "weekday_start 必须早于 weekday_end");
    }
    if (config.weekend_start >= config.weekend_end) {
        return Result<ScheduleConfig>::fail(ErrorCode::InvalidValue,
                                            "weekend_start 必须早于 weekend_end");
    }
    if (config.min_block > 24 * 60) {
        return Result<ScheduleConfig>::fail(ErrorCode::InvalidValue,
                                            "min_block 不能超过一天");
    }

    return Result<ScheduleConfig>::ok(config);
}

}  // namespace ordo
