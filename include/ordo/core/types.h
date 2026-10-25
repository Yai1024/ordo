#pragma once

// 全项目共用的基础类型与纯工具函数（OUTLINE §6）。
// 这一层不认识 Task、不认识文件，只提供"时间"和"文本"的基础工具。

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ordo {

using TaskId = std::uint64_t;
using Minutes = std::int32_t;  // 时长与偏移的统一单位：分钟
using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

constexpr Minutes kMinutesPerDay = 24 * 60;
constexpr Minutes kMinutesPerHour = 60;

// ---- 时间 ----
// 内部一律用 TimePoint；只在输入输出边界转换成本地时间字符串（OUTLINE §8.3）。
// 接受 "YYYY-MM-DD HH:MM" 与 "YYYY-MM-DD" 两种格式；
// 只给日期时按当天 23:59 处理，符合"当天截止"的日常语义。
std::optional<TimePoint> parse_datetime(std::string_view text);

// 事件日志专用：带秒的 "YYYY-MM-DD HH:MM:SS"（ADR-0004）。
std::optional<TimePoint> parse_datetime_seconds(std::string_view text);

// 固定格式 "YYYY-MM-DD HH:MM" / "YYYY-MM-DD" / "HH:MM"。
std::string format_datetime(TimePoint point);
std::string format_datetime_seconds(TimePoint point);
std::string format_date(TimePoint point);
std::string format_time(TimePoint point);

// to - from 的分钟数，可为负。
Minutes minutes_between(TimePoint from, TimePoint to);

TimePoint add_minutes(TimePoint point, Minutes minutes);

// ---- 文本 ----
// 去掉首尾空白（空格、制表符、回车、换行），返回原字符串的视图。
std::string_view trim(std::string_view text);

}  // namespace ordo
