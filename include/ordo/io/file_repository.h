#pragma once

// CSV 文件实现（OUTLINE §7.3、§9.1）。
// 写入采用"临时文件 + 原子替换"：任何一步失败都保留上一份完整数据。

#include <filesystem>
#include <vector>

#include "ordo/core/task.h"
#include "ordo/io/repository.h"

namespace ordo {

class CsvTaskRepository final : public ITaskRepository {
public:
    explicit CsvTaskRepository(std::filesystem::path path);

    Result<std::vector<Task>> load_all() const override;
    Status save_all(const std::vector<Task>& tasks) override;
    Result<Task> find(TaskId id) const override;
    Status upsert(const Task& task) override;
    Status erase(TaskId id) override;

    const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

}  // namespace ordo
