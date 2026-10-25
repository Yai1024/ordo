#include "ordo/io/event_log.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ordo/core/types.h"
#include "ordo/io/csv.h"

namespace ordo {
namespace {

constexpr std::string_view kHeader = "timestamp,command,task_id,minutes,note";
constexpr std::size_t kColumnCount = 5;

bool parse_u64(std::string_view text, std::uint64_t& out) {
    if (text.empty()) {
        return false;
    }
    const std::from_chars_result result =
        std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool parse_int(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }
    const std::from_chars_result result =
        std::from_chars(text.data(), text.data() + text.size(), out);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

std::string line_prefix(std::size_t line_number) {
    return "第 " + std::to_string(line_number) + " 行: ";
}

Result<std::vector<LogEntry>> fail_entries(ErrorCode code, std::string message) {
    return Result<std::vector<LogEntry>>::fail(code, std::move(message));
}

}  // namespace

CsvEventLog::CsvEventLog(std::filesystem::path path) : path_(std::move(path)) {}

Status CsvEventLog::append(const LogEntry& entry) {
    if (trim(entry.command).empty()) {
        return Status::fail(ErrorCode::EmptyInput, "日志命令不能为空");
    }
    if (entry.minutes.has_value() && *entry.minutes < 0) {
        return Status::fail(ErrorCode::InvalidValue, "日志投入分钟不能为负");
    }

    if (path_.has_parent_path()) {
        std::error_code dir_error;
        std::filesystem::create_directories(path_.parent_path(), dir_error);
        if (dir_error) {
            return Status::fail(ErrorCode::IoError,
                                "无法创建日志目录: " + dir_error.message());
        }
    }

    // app 模式下 tellp 不可靠（首次写入前可能报 0），直接看文件大小。
    bool need_header = true;
    std::error_code size_error;
    const std::uintmax_t size = std::filesystem::file_size(path_, size_error);
    if (!size_error && size > 0) {
        need_header = false;
    }

    std::ofstream output(path_, std::ios::binary | std::ios::app);
    if (!output) {
        return Status::fail(ErrorCode::IoError, "无法追加日志文件: " + path_.string());
    }
    if (need_header) {
        output << kHeader << '\n';
    }

    const std::vector<std::string> fields{
        format_datetime_seconds(entry.timestamp),
        entry.command,
        entry.task_id.has_value() ? std::to_string(*entry.task_id) : std::string{},
        entry.minutes.has_value() ? std::to_string(*entry.minutes) : std::string{},
        entry.note,
    };
    output << csv::join_fields(fields) << '\n';
    output.flush();

    if (!output) {
        return Status::fail(ErrorCode::IoError, "写日志失败: " + path_.string());
    }
    return Status::success();
}

Result<std::vector<LogEntry>> CsvEventLog::read() const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        if (!std::filesystem::exists(path_)) {
            return Result<std::vector<LogEntry>>::ok({});
        }
        return fail_entries(ErrorCode::IoError, "无法打开日志文件: " + path_.string());
    }

    std::vector<LogEntry> entries;
    std::string line;
    std::size_t line_number = 0;
    bool header_seen = false;

    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string_view view{line};
        if (!header_seen) {
            view = csv::strip_bom(view);
        }
        if (trim(view).empty()) {
            continue;
        }

        if (!header_seen) {
            if (view != kHeader) {
                return fail_entries(ErrorCode::Corrupted,
                                    line_prefix(line_number) + "日志表头不匹配，应为: " +
                                        std::string{kHeader});
            }
            header_seen = true;
            continue;
        }

        auto parsed = csv::parse_line(view);
        if (!parsed.has_value()) {
            return fail_entries(ErrorCode::Corrupted,
                                line_prefix(line_number) + parsed.status().message);
        }
        std::vector<std::string> fields = std::move(parsed.value());
        if (fields.size() != kColumnCount) {
            return fail_entries(ErrorCode::Corrupted,
                                line_prefix(line_number) + "字段数为 " +
                                    std::to_string(fields.size()) + "，应为 " +
                                    std::to_string(kColumnCount));
        }
        for (std::string& field : fields) {
            field = std::string{trim(field)};
        }

        LogEntry entry;
        const std::optional<TimePoint> timestamp = parse_datetime_seconds(fields[0]);
        if (!timestamp.has_value()) {
            return fail_entries(ErrorCode::InvalidFormat,
                                line_prefix(line_number) +
                                    "timestamp 格式应为 YYYY-MM-DD HH:MM:SS: " + fields[0]);
        }
        entry.timestamp = *timestamp;

        if (fields[1].empty()) {
            return fail_entries(ErrorCode::InvalidFormat,
                                line_prefix(line_number) + "command 不能为空");
        }
        entry.command = fields[1];

        if (!fields[2].empty()) {
            TaskId task_id = 0;
            if (!parse_u64(fields[2], task_id) || task_id == 0) {
                return fail_entries(ErrorCode::InvalidFormat,
                                    line_prefix(line_number) +
                                        "task_id 不是合法正整数: " + fields[2]);
            }
            entry.task_id = task_id;
        }

        if (!fields[3].empty()) {
            int minutes = 0;
            if (!parse_int(fields[3], minutes) || minutes < 0) {
                return fail_entries(ErrorCode::InvalidFormat,
                                    line_prefix(line_number) +
                                        "minutes 不是合法非负整数: " + fields[3]);
            }
            entry.minutes = static_cast<Minutes>(minutes);
        }

        entry.note = fields[4];
        entries.push_back(std::move(entry));
    }

    if (input.bad()) {
        return fail_entries(ErrorCode::IoError, "读取日志文件失败: " + path_.string());
    }
    return Result<std::vector<LogEntry>>::ok(std::move(entries));
}

}  // namespace ordo
