// M2：config.ini 解析（ADR-0003：缺省、非法值、时间解析）。

#include <filesystem>
#include <fstream>
#include <string>

#include "ordo/io/config.h"
#include "test_framework.h"

namespace {

std::filesystem::path temp_file(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

}  // namespace

ORDO_TEST(config_missing_file_uses_defaults) {
    const std::filesystem::path path = temp_file("ordo_config_missing.ini");
    std::filesystem::remove(path);

    const auto config = ordo::load_config(path);
    ORDO_CHECK(config.has_value());
    ORDO_CHECK_EQ(config.value().weekday_start, ordo::Minutes{18 * 60 + 30});
    ORDO_CHECK_EQ(config.value().weekday_end, ordo::Minutes{23 * 60});
    ORDO_CHECK_EQ(config.value().weekend_start, ordo::Minutes{9 * 60});
    ORDO_CHECK_EQ(config.value().break_minutes, ordo::Minutes{10});
    ORDO_CHECK(config.value().allow_split);
    ORDO_CHECK_EQ(config.value().min_block, ordo::Minutes{15});
}

ORDO_TEST(config_reads_schedule_and_policy) {
    const std::filesystem::path path = temp_file("ordo_config_full.ini");
    write_text(path,
               "# 注释\n"
               "[schedule]\n"
               "weekday_start = 08:00\n"
               "weekday_end   = 12:00\n"
               "weekend_start = 9:05\n"
               "weekend_end   = 18:30\n"
               "break_minutes = 5\n"
               "[policy]\n"
               "default_estimate = 45\n"
               "allow_split      = false\n"
               "min_block        = 20\n"
               "unknown_key      = ignored\n");

    const auto config = ordo::load_config(path);
    ORDO_CHECK(config.has_value());
    ORDO_CHECK_EQ(config.value().weekday_start, ordo::Minutes{8 * 60});
    ORDO_CHECK_EQ(config.value().weekday_end, ordo::Minutes{12 * 60});
    ORDO_CHECK_EQ(config.value().weekend_start, ordo::Minutes{9 * 60 + 5});
    ORDO_CHECK_EQ(config.value().weekend_end, ordo::Minutes{18 * 60 + 30});
    ORDO_CHECK_EQ(config.value().break_minutes, ordo::Minutes{5});
    ORDO_CHECK_EQ(config.value().default_estimate, ordo::Minutes{45});
    ORDO_CHECK(!config.value().allow_split);
    ORDO_CHECK_EQ(config.value().min_block, ordo::Minutes{20});

    std::filesystem::remove(path);
}

ORDO_TEST(config_rejects_bad_time_with_line_number) {
    const std::filesystem::path path = temp_file("ordo_config_badtime.ini");
    write_text(path, "[schedule]\nweekday_start = 25:00\n");

    const auto config = ordo::load_config(path);
    ORDO_CHECK(!config.has_value());
    ORDO_CHECK(config.status().code == ordo::ErrorCode::InvalidFormat);
    ORDO_CHECK(config.status().message.find("第 2 行") != std::string::npos);

    std::filesystem::remove(path);
}

ORDO_TEST(config_rejects_reversed_window) {
    const std::filesystem::path path = temp_file("ordo_config_reversed.ini");
    write_text(path, "[schedule]\nweekday_start = 23:00\nweekday_end = 18:30\n");

    const auto config = ordo::load_config(path);
    ORDO_CHECK(!config.has_value());
    ORDO_CHECK(config.status().code == ordo::ErrorCode::InvalidValue);

    std::filesystem::remove(path);
}

ORDO_TEST(config_rejects_bad_boolean) {
    const std::filesystem::path path = temp_file("ordo_config_bool.ini");
    write_text(path, "[policy]\nallow_split = maybe\n");

    const auto config = ordo::load_config(path);
    ORDO_CHECK(!config.has_value());
    ORDO_CHECK(config.status().code == ordo::ErrorCode::InvalidFormat);

    std::filesystem::remove(path);
}

ORDO_TEST(config_tolerates_bom_and_crlf) {
    const std::filesystem::path path = temp_file("ordo_config_bom.ini");
    write_text(path,
               "\xEF\xBB\xBF[schedule]\r\n"
               "weekday_start = 7:05\r\n"
               "weekday_end = 9:00\r\n");

    const auto config = ordo::load_config(path);
    ORDO_CHECK(config.has_value());
    ORDO_CHECK_EQ(config.value().weekday_start, ordo::Minutes{7 * 60 + 5});
    ORDO_CHECK_EQ(config.value().weekday_end, ordo::Minutes{9 * 60});

    std::filesystem::remove(path);
}
