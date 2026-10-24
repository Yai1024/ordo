#pragma once

// 新增/编辑任务对话框（Win32 模态窗口）。
// 只负责收集表单并交给 ui::AppModel；不写业务校验。

#include <optional>

#include "ordo/gui/win32_util.h"
#include "ordo/ui/app_model.h"

namespace ordo::gui {

// edit_id 为空表示新增，否则编辑该任务。
// 返回 true 表示用户保存成功，调用方应刷新列表。
bool show_task_dialog(HWND parent, int dpi, AppModel& model, std::optional<TaskId> edit_id);

}  // namespace ordo::gui
