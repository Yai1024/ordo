#pragma once

// Ordo 主窗口（Win32 适配器）。
// 只做三件事：摆放控件、把用户操作转成 AppModel 调用、显示结果。

#include <string>

#include "ordo/gui/win32_util.h"
#include "ordo/ui/app_model.h"

namespace ordo::gui {

class MainWindow {
public:
    MainWindow(ITaskRepository& repo, ScheduleConfig config);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool create(HINSTANCE instance, int show_command);
    int run();

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT handle(UINT message, WPARAM wparam, LPARAM lparam);

    void create_children();
    void layout();
    void refresh();
    void update_stats();
    void update_plan();
    void update_buttons();
    void set_status(const std::wstring& text);

    ListQuery current_query() const;
    TaskId selected_id() const;

    void on_add();
    void on_edit();
    void on_done();
    void on_remove();

    HWND hwnd_{};
    HWND list_{};
    HWND add_button_{};
    HWND edit_button_{};
    HWND done_button_{};
    HWND remove_button_{};
    HWND refresh_button_{};
    HWND state_label_{};
    HWND rank_label_{};
    HWND state_combo_{};
    HWND rank_combo_{};
    HWND plan_text_{};
    HWND stats_text_{};
    HWND status_{};

    HFONT font_{};
    HFONT title_font_{};
    HFONT heading_font_{};
    HFONT small_font_{};
    HBRUSH background_{};

    int dpi_{96};
    HINSTANCE instance_{};
    AppModel model_;
};

}  // namespace ordo::gui
