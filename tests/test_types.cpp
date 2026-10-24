// M1：时间字符串与工具的边界行为（OUTLINE §8.3）。

#include <chrono>
#include <string>

#include "ordo/core/types.h"
#include "test_framework.h"

ORDO_TEST(datetime_round_trips_through_text) {
    const auto parsed = ordo::parse_datetime("2026-10-30 23:59");
    ORDO_CHECK(parsed.has_value());
    ORDO_CHECK_EQ(ordo::format_datetime(*parsed), std::string{"2026-10-30 23:59"});
    ORDO_CHECK_EQ(ordo::format_date(*parsed), std::string{"2026-10-30"});
}

ORDO_TEST(date_only_means_end_of_day) {
    const auto date_only = ordo::parse_datetime("2026-10-30");
    const auto end_of_day = ordo::parse_datetime("2026-10-30 23:59");
    ORDO_CHECK(date_only.has_value());
    ORDO_CHECK(end_of_day.has_value());
    ORDO_CHECK(*date_only == *end_of_day);
}

ORDO_TEST(invalid_datetime_is_rejected) {
    ORDO_CHECK(!ordo::parse_datetime("").has_value());
    ORDO_CHECK(!ordo::parse_datetime("2026/10/30 23:59").has_value());
    ORDO_CHECK(!ordo::parse_datetime("2026-13-01 10:00").has_value());
    ORDO_CHECK(!ordo::parse_datetime("2026-02-30 10:00").has_value());  // mktime 会归一化
    ORDO_CHECK(!ordo::parse_datetime("2026-10-30 24:00").has_value());
    ORDO_CHECK(!ordo::parse_datetime("2026-10-30 23:60").has_value());
    ORDO_CHECK(!ordo::parse_datetime("not a time").has_value());
}

ORDO_TEST(minutes_between_and_add_minutes) {
    const auto begin = ordo::parse_datetime("2026-10-30 08:00");
    const auto end = ordo::parse_datetime("2026-10-30 09:30");
    ORDO_CHECK(begin.has_value());
    ORDO_CHECK(end.has_value());
    ORDO_CHECK_EQ(ordo::minutes_between(*begin, *end), ordo::Minutes{90});
    ORDO_CHECK_EQ(ordo::minutes_between(*end, *begin), ordo::Minutes{-90});
    ORDO_CHECK(ordo::add_minutes(*begin, 90) == *end);
    ORDO_CHECK(ordo::add_minutes(*begin, 0) == *begin);
}

ORDO_TEST(trim_handles_whitespace_and_empty) {
    ORDO_CHECK_EQ(ordo::trim("  abc \t"), std::string_view{"abc"});
    ORDO_CHECK_EQ(ordo::trim("\r\n abc \n"), std::string_view{"abc"});
    ORDO_CHECK(ordo::trim("   ").empty());
    ORDO_CHECK(ordo::trim("").empty());
}
