#pragma once

// GUI 无关的界面模型（ADR-0002）。
// 这是未来任何外部 GUI（Qt / Web / ImGui）唯一的接入点：
//   - 只出现 std::string（UTF-8）、Result<T>、Status；
//   - 不出现 HWND、wchar_t、窗口消息、控件句柄；
//   - 业务规则仍由 app::TaskService 负责，本层只做表单解析与展示映射。

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "ordo/app/task_service.h"
#include "ordo/core/result.h"
#include "ordo/core/task.h"
#include "ordo/core/types.h"
#include "ordo/io/repository.h"

namespace ordo {

struct ListQuery {
    std::optional<TaskState> state;  // 指定状态；空表示不按状态过滤
    bool open_only{true};            // state 为空时，是否只保留 todo/doing
    RankMode rank{RankMode::None};
};

// 列表一行。字符串已经格式化好，GUI 只负责显示。
struct TaskRow {
    TaskId id{};
    std::string title;
    std::string course;
    int importance{0};
    Minutes estimate{0};
    std::string deadline_text;  // YYYY-MM-DD HH:MM
    TaskState state{TaskState::Todo};
};

// 新增/编辑表单的原始输入。解析与校验在本层完成，GUI 不写业务判断。
struct TaskForm {
    std::string title;
    std::string course;
    std::string importance{"3"};
    std::string estimate{"0"};
    std::string deadline;              // YYYY-MM-DD 或 YYYY-MM-DD HH:MM
    std::optional<TaskState> state;    // 新增时忽略；编辑时保留原状态
};

class AppModel {
public:
    AppModel(ITaskRepository& repo, std::function<TimePoint()> clock);

    Result<std::vector<TaskRow>> list(const ListQuery& query) const;
    Result<Stats> stats() const;

    // 编辑对话框的初始值。
    Result<TaskForm> form_for(TaskId id) const;

    Result<TaskId> create(const TaskForm& form);
    Status update(TaskId id, const TaskForm& form);
    Status remove(TaskId id);
    Status set_state(TaskId id, TaskState state);

private:
    Result<Task> build_task(TaskId id, const TaskForm& form) const;

    ITaskRepository& repo_;
    std::function<TimePoint()> clock_;
};

}  // namespace ordo
