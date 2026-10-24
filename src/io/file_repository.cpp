#include "ordo/io/file_repository.h"

#include <algorithm>
#include <cstddef>
#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>

#include "ordo/core/types.h"
#include "ordo/io/csv.h"

namespace ordo {
namespace {

constexpr std::string_view kHeader =
    "id,title,course,importance,estimate,deadline,state,review_stage";
constexpr std::size_t kColumnCount = 8;

bool parse_u64(std::string_view text, std::uint64_t& out) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last;
}

bool parse_int(std::string_view text, int& out) {
    if (text.empty()) {
        return false;
    }
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last;
}

Result<std::vector<Task>> fail_tasks(ErrorCode code, std::string message) {
    return Result<std::vector<Task>>::fail(code, std::move(message));
}

std::string line_prefix(std::size_t line_number) {
    return "第 " + std::to_string(line_number) + " 行: ";
}

}  // namespace

CsvTaskRepository::CsvTaskRepository(std::filesystem::path path) : path_(std::move(path)) {}

Result<std::vector<Task>> CsvTaskRepository::load_all() const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        // 首次运行：文件不存在视为空表，不报错（OUTLINE §8.4）。
        if (!std::filesystem::exists(path_)) {
            return Result<std::vector<Task>>::ok({});
        }
        return fail_tasks(ErrorCode::IoError, "无法打开数据文件: " + path_.string());
    }

    std::vector<Task> tasks;
    std::unordered_set<TaskId> seen_ids;
    std::string line;
    std::size_t line_number = 0;
    bool header_seen = false;

    while (std::getline(input, line)) {
        ++line_number;

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // 容忍 CRLF（OUTLINE §9.4）
        }

        std::string_view view{line};
        if (!header_seen) {
            view = csv::strip_bom(view);
        }
        if (trim(view).empty()) {
            continue;  // 空行跳过
        }

        if (!header_seen) {
            if (view != kHeader) {
                return fail_tasks(ErrorCode::Corrupted,
                                  line_prefix(line_number) + "表头不匹配，应为: " +
                                      std::string{kHeader});
            }
            header_seen = true;
            continue;
        }

        auto parsed = csv::parse_line(view);
        if (!parsed.has_value()) {
            return fail_tasks(ErrorCode::Corrupted,
                              line_prefix(line_number) + parsed.status().message);
        }

        std::vector<std::string> fields = std::move(parsed.value());
        if (fields.size() != kColumnCount) {
            return fail_tasks(ErrorCode::Corrupted,
                              line_prefix(line_number) + "字段数为 " +
                                  std::to_string(fields.size()) + "，应为 " +
                                  std::to_string(kColumnCount));
        }
        for (std::string& field : fields) {
            field = std::string{trim(field)};
        }

        Task task;
        task.title = fields[1];
        task.course = fields[2];

        int importance = 0;
        int estimate = 0;
        int review_stage = 0;
        const std::optional<TaskState> state = parse_task_state(fields[6]);
        const std::optional<TimePoint> deadline = parse_datetime(fields[5]);

        if (!parse_u64(fields[0], task.id)) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) + "id 不是合法整数: " + fields[0]);
        }
        if (!parse_int(fields[3], importance)) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) + "importance 不是合法整数: " + fields[3]);
        }
        if (!parse_int(fields[4], estimate)) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) + "estimate 不是合法整数: " + fields[4]);
        }
        if (!deadline.has_value()) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) + "deadline 格式应为 YYYY-MM-DD HH:MM: " +
                                  fields[5]);
        }
        if (!state.has_value()) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) +
                                  "state 只能是 todo/doing/done/dropped: " + fields[6]);
        }
        if (!parse_int(fields[7], review_stage)) {
            return fail_tasks(ErrorCode::InvalidFormat,
                              line_prefix(line_number) + "review_stage 不是合法整数: " + fields[7]);
        }

        task.importance = importance;
        task.estimate = static_cast<Minutes>(estimate);
        task.deadline = *deadline;
        task.state = *state;
        task.review_stage = review_stage;

        const Status check = task.validate();
        if (!check.ok()) {
            return fail_tasks(check.code, line_prefix(line_number) + check.message);
        }

        if (!seen_ids.insert(task.id).second) {
            return fail_tasks(ErrorCode::Duplicate,
                              line_prefix(line_number) + "任务 id " + std::to_string(task.id) +
                                  " 重复");
        }

        tasks.push_back(std::move(task));
    }

    if (input.bad()) {
        return fail_tasks(ErrorCode::IoError, "读取数据文件失败: " + path_.string());
    }

    return Result<std::vector<Task>>::ok(std::move(tasks));
}

Status CsvTaskRepository::save_all(const std::vector<Task>& tasks) {
    for (const Task& task : tasks) {
        const Status check = task.validate();
        if (!check.ok()) {
            return Status::fail(check.code,
                                "任务 #" + std::to_string(task.id) + ": " + check.message);
        }
    }

    if (path_.has_parent_path()) {
        std::error_code dir_error;
        std::filesystem::create_directories(path_.parent_path(), dir_error);
        if (dir_error) {
            return Status::fail(ErrorCode::IoError,
                                "无法创建数据目录: " + dir_error.message());
        }
    }

    std::filesystem::path temp_path = path_;
    temp_path += ".tmp";

    std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return Status::fail(ErrorCode::IoError, "无法写入临时文件: " + temp_path.string());
    }

    output << kHeader << '\n';
    for (const Task& task : tasks) {
        const std::vector<std::string> fields{
            std::to_string(task.id),
            task.title,
            task.course,
            std::to_string(task.importance),
            std::to_string(task.estimate),
            format_datetime(task.deadline),
            std::string{to_string(task.state)},
            std::to_string(task.review_stage),
        };
        output << csv::join_fields(fields) << '\n';
    }

    output.flush();
    if (!output) {
        output.close();
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return Status::fail(ErrorCode::IoError, "写入数据文件失败: " + temp_path.string());
    }
    output.close();
    if (!output) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return Status::fail(ErrorCode::IoError, "关闭数据文件失败: " + temp_path.string());
    }

    // 原子替换：失败时绝不留半截文件（OUTLINE §8.4）。
    std::error_code rename_error;
    std::filesystem::rename(temp_path, path_, rename_error);
    if (rename_error) {
        // Windows 上目标已存在时，部分实现会拒绝直接改名；退化为先删后改名。
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        rename_error.clear();
        std::filesystem::rename(temp_path, path_, rename_error);
    }
    if (rename_error) {
        std::error_code ignored;
        std::filesystem::remove(temp_path, ignored);
        return Status::fail(ErrorCode::IoError,
                            "无法替换数据文件: " + rename_error.message());
    }

    return Status::success();
}

Result<Task> CsvTaskRepository::find(TaskId id) const {
    auto loaded = load_all();
    if (!loaded.has_value()) {
        return Result<Task>::fail(loaded.status().code, loaded.status().message);
    }
    for (const Task& task : loaded.value()) {
        if (task.id == id) {
            return Result<Task>::ok(task);
        }
    }
    return Result<Task>::fail(ErrorCode::NotFound, "找不到任务 #" + std::to_string(id));
}

Status CsvTaskRepository::upsert(const Task& task) {
    auto loaded = load_all();
    if (!loaded.has_value()) {
        return loaded.status();
    }

    std::vector<Task> tasks = std::move(loaded.value());
    bool replaced = false;
    for (Task& existing : tasks) {
        if (existing.id == task.id) {
            existing = task;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        tasks.push_back(task);
    }

    return save_all(tasks);
}

Status CsvTaskRepository::erase(TaskId id) {
    auto loaded = load_all();
    if (!loaded.has_value()) {
        return loaded.status();
    }

    std::vector<Task> tasks = std::move(loaded.value());
    const auto position = std::find_if(tasks.begin(), tasks.end(),
                                       [id](const Task& task) { return task.id == id; });
    if (position == tasks.end()) {
        return Status::fail(ErrorCode::NotFound, "找不到任务 #" + std::to_string(id));
    }
    tasks.erase(position);

    return save_all(tasks);
}

}  // namespace ordo
