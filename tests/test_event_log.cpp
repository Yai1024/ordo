// M3：事件日志的追加、往返与损坏文件（OUTLINE §11、ADR-0004）。

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

#include "ordo/io/event_log.h"
#include "test_framework.h"

namespace {

std::filesystem::path temp_file(const std::string& name) {
    return std::filesystem::temp_directory_path() / name;
}

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

const std::string kHeader = "timestamp,command,task_id,minutes,note\n";

ordo::LogEntry make_entry(std::string command) {
    ordo::LogEntry entry;
    entry.timestamp = *ordo::parse_datetime_seconds("2026-10-25 20:15:03");
    entry.command = std::move(command);
    entry.task_id = ordo::TaskId{7};
    entry.minutes = ordo::Minutes{45};
    entry.note = "交税收作业";
    return entry;
}

}  // namespace

ORDO_TEST(event_log_append_creates_header_and_round_trips) {
    const std::filesystem::path path = temp_file("ordo_event_log_roundtrip.csv");
    std::filesystem::remove(path);

    ordo::CsvEventLog log(path);
    ORDO_CHECK(log.append(make_entry("task.done")).ok());

    const auto entries = log.read();
    ORDO_CHECK(entries.has_value());
    ORDO_CHECK_EQ(entries.value().size(), std::size_t{1});

    const ordo::LogEntry& entry = entries.value()[0];
    ORDO_CHECK_EQ(ordo::format_datetime_seconds(entry.timestamp),
                  std::string{"2026-10-25 20:15:03"});
    ORDO_CHECK_EQ(entry.command, std::string{"task.done"});
    ORDO_CHECK(entry.task_id.has_value());
    ORDO_CHECK_EQ(*entry.task_id, ordo::TaskId{7});
    ORDO_CHECK(entry.minutes.has_value());
    ORDO_CHECK_EQ(*entry.minutes, ordo::Minutes{45});
    ORDO_CHECK_EQ(entry.note, std::string{"交税收作业"});

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_appends_without_duplicating_header) {
    const std::filesystem::path path = temp_file("ordo_event_log_append.csv");
    std::filesystem::remove(path);

    ordo::CsvEventLog log(path);
    ORDO_CHECK(log.append(make_entry("task.add")).ok());
    ORDO_CHECK(log.append(make_entry("task.review")).ok());
    ORDO_CHECK(log.append(make_entry("task.remove")).ok());

    const auto entries = log.read();
    ORDO_CHECK(entries.has_value());
    ORDO_CHECK_EQ(entries.value().size(), std::size_t{3});

    std::ifstream input(path, std::ios::binary);
    std::string line;
    int header_count = 0;
    while (std::getline(input, line)) {
        if (line == "timestamp,command,task_id,minutes,note\r" ||
            line == "timestamp,command,task_id,minutes,note") {
            ++header_count;
        }
    }
    ORDO_CHECK_EQ(header_count, 1);
    input.close();

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_escapes_note) {
    const std::filesystem::path path = temp_file("ordo_event_log_escape.csv");
    std::filesystem::remove(path);

    ordo::LogEntry entry = make_entry("task.edit");
    entry.note = "他说\"好\", 第3章";
    entry.task_id.reset();
    entry.minutes.reset();

    ordo::CsvEventLog log(path);
    ORDO_CHECK(log.append(entry).ok());

    const auto entries = log.read();
    ORDO_CHECK(entries.has_value());
    ORDO_CHECK_EQ(entries.value().size(), std::size_t{1});
    ORDO_CHECK_EQ(entries.value()[0].note, std::string{"他说\"好\", 第3章"});
    ORDO_CHECK(!entries.value()[0].task_id.has_value());
    ORDO_CHECK(!entries.value()[0].minutes.has_value());

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_missing_file_is_empty) {
    const std::filesystem::path path = temp_file("ordo_event_log_missing.csv");
    std::filesystem::remove(path);

    const ordo::CsvEventLog log(path);
    const auto entries = log.read();
    ORDO_CHECK(entries.has_value());
    ORDO_CHECK(entries.value().empty());
}

ORDO_TEST(event_log_rejects_bad_header) {
    const std::filesystem::path path = temp_file("ordo_event_log_badheader.csv");
    write_text(path, "time,command\n2026-10-25 20:15:03,task.done\n");

    const ordo::CsvEventLog log(path);
    const auto entries = log.read();
    ORDO_CHECK(!entries.has_value());
    ORDO_CHECK(entries.status().code == ordo::ErrorCode::Corrupted);

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_rejects_bad_timestamp) {
    const std::filesystem::path path = temp_file("ordo_event_log_badtime.csv");
    write_text(path, kHeader + "2026-10-25 20:15,task.done,1,,x\n");

    const ordo::CsvEventLog log(path);
    const auto entries = log.read();
    ORDO_CHECK(!entries.has_value());
    ORDO_CHECK(entries.status().code == ordo::ErrorCode::InvalidFormat);

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_rejects_negative_minutes) {
    const std::filesystem::path path = temp_file("ordo_event_log_badminutes.csv");
    write_text(path, kHeader + "2026-10-25 20:15:03,task.done,1,-5,x\n");

    const ordo::CsvEventLog log(path);
    const auto entries = log.read();
    ORDO_CHECK(!entries.has_value());
    ORDO_CHECK(entries.status().code == ordo::ErrorCode::InvalidFormat);

    std::filesystem::remove(path);
}

ORDO_TEST(event_log_append_rejects_negative_minutes) {
    const std::filesystem::path path = temp_file("ordo_event_log_append_bad.csv");
    std::filesystem::remove(path);

    ordo::LogEntry entry = make_entry("task.review");
    entry.minutes = ordo::Minutes{-1};

    ordo::CsvEventLog log(path);
    const ordo::Status status = log.append(entry);
    ORDO_CHECK(!status.ok());
    ORDO_CHECK(status.code == ordo::ErrorCode::InvalidValue);
    ORDO_CHECK(!std::filesystem::exists(path));
}
