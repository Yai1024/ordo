#pragma once

// 事件日志：只追加的写操作审计（OUTLINE §9.3、ADR-0004）。
// 与任务仓库分开：日志不是业务数据，写入失败也不回滚任务操作。

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ordo/core/result.h"
#include "ordo/core/types.h"

namespace ordo {

struct LogEntry {
    TimePoint timestamp{};
    std::string command;           // task.add / task.edit / task.done / task.remove / task.review
    std::optional<TaskId> task_id;  // 任务删除后仍保留
    std::optional<Minutes> minutes; // 实际投入；未给出时为空
    std::string note;
};

class IEventLog {
public:
    virtual ~IEventLog() = default;

    // 追加一行；文件不存在或为空时先写表头。
    virtual Status append(const LogEntry& entry) = 0;

    // 读取全部事件；文件不存在视为空日志。
    virtual Result<std::vector<LogEntry>> read() const = 0;
};

class CsvEventLog final : public IEventLog {
public:
    explicit CsvEventLog(std::filesystem::path path);

    Status append(const LogEntry& entry) override;
    Result<std::vector<LogEntry>> read() const override;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace ordo
