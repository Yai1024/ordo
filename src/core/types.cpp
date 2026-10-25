#include "ordo/core/types.h"

#include <cstdio>
#include <ctime>

namespace ordo {
namespace {

bool all_digits(std::string_view text) {
    if (text.empty()) {
        return false;
    }
    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

int digits_to_int(std::string_view text) {
    int value = 0;
    for (const char ch : text) {
        value = value * 10 + (ch - '0');
    }
    return value;
}

bool has_separator(std::string_view text, std::size_t index, char expected) {
    return index < text.size() && text[index] == expected;
}

}  // namespace

std::string_view trim(std::string_view text) {
    constexpr std::string_view kWhitespace = " \t\r\n";
    const std::size_t begin = text.find_first_not_of(kWhitespace);
    if (begin == std::string_view::npos) {
        return {};
    }
    const std::size_t end = text.find_last_not_of(kWhitespace);
    return text.substr(begin, end - begin + 1);
}

std::optional<TimePoint> parse_datetime(std::string_view text) {
    text = trim(text);

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 23;  // 只给日期时视为当天 23:59
    int minute = 59;

    if (text.size() == 16) {
        if (!has_separator(text, 4, '-') || !has_separator(text, 7, '-') ||
            !has_separator(text, 10, ' ') || !has_separator(text, 13, ':')) {
            return std::nullopt;
        }
        const std::string_view hhmm = text.substr(11, 5);
        if (!all_digits(text.substr(0, 4)) || !all_digits(text.substr(5, 2)) ||
            !all_digits(text.substr(8, 2)) || !all_digits(hhmm.substr(0, 2)) ||
            !all_digits(hhmm.substr(3, 2))) {
            return std::nullopt;
        }
        year = digits_to_int(text.substr(0, 4));
        month = digits_to_int(text.substr(5, 2));
        day = digits_to_int(text.substr(8, 2));
        hour = digits_to_int(hhmm.substr(0, 2));
        minute = digits_to_int(hhmm.substr(3, 2));
    } else if (text.size() == 10) {
        if (!has_separator(text, 4, '-') || !has_separator(text, 7, '-') ||
            !all_digits(text.substr(0, 4)) || !all_digits(text.substr(5, 2)) ||
            !all_digits(text.substr(8, 2))) {
            return std::nullopt;
        }
        year = digits_to_int(text.substr(0, 4));
        month = digits_to_int(text.substr(5, 2));
        day = digits_to_int(text.substr(8, 2));
    } else {
        return std::nullopt;
    }

    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
        minute > 59) {
        return std::nullopt;
    }

    std::tm parts{};
    parts.tm_year = year - 1900;
    parts.tm_mon = month - 1;
    parts.tm_mday = day;
    parts.tm_hour = hour;
    parts.tm_min = minute;
    parts.tm_sec = 0;
    parts.tm_isdst = -1;

    const std::time_t seconds = std::mktime(&parts);
    if (seconds == static_cast<std::time_t>(-1)) {
        return std::nullopt;
    }

    // mktime 会把 02-30 这类日期悄悄归一化到 03-02；逐字段回读可识别并拒绝。
    if (parts.tm_year != year - 1900 || parts.tm_mon != month - 1 || parts.tm_mday != day ||
        parts.tm_hour != hour || parts.tm_min != minute) {
        return std::nullopt;
    }

    return Clock::from_time_t(seconds);
}

std::optional<TimePoint> parse_datetime_seconds(std::string_view text) {
    text = trim(text);
    if (text.size() != 19 || !has_separator(text, 16, ':')) {
        return std::nullopt;
    }
    const std::string_view seconds_text = text.substr(17, 2);
    if (!all_digits(seconds_text)) {
        return std::nullopt;
    }
    const int seconds = digits_to_int(seconds_text);
    if (seconds > 59) {
        return std::nullopt;
    }
    // 日期与分钟部分复用 parse_datetime，秒只做加法；这样闰日与归一化检查只有一份。
    const std::optional<TimePoint> minute_precision = parse_datetime(text.substr(0, 16));
    if (!minute_precision.has_value()) {
        return std::nullopt;
    }
    return *minute_precision + std::chrono::seconds(seconds);
}

namespace {

std::tm to_local(TimePoint point) {
    const std::time_t seconds = Clock::to_time_t(point);
    std::tm result{};
    if (const std::tm* local = std::localtime(&seconds)) {
        result = *local;
    }
    return result;
}

}  // namespace

std::string format_datetime(TimePoint point) {
    const std::tm local = to_local(point);
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
    return buffer;
}

std::string format_datetime_seconds(TimePoint point) {
    const std::tm local = to_local(point);
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                  local.tm_min, local.tm_sec);
    return buffer;
}

std::string format_date(TimePoint point) {
    const std::tm local = to_local(point);
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", local.tm_year + 1900,
                  local.tm_mon + 1, local.tm_mday);
    return buffer;
}

std::string format_time(TimePoint point) {
    const std::tm local = to_local(point);
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", local.tm_hour, local.tm_min);
    return buffer;
}

Minutes minutes_between(TimePoint from, TimePoint to) {
    return static_cast<Minutes>(
        std::chrono::duration_cast<std::chrono::minutes>(to - from).count());
}

TimePoint add_minutes(TimePoint point, Minutes minutes) {
    return point + std::chrono::minutes{minutes};
}

}  // namespace ordo
