#include "ordo/gui/main_window.h"

#include <commctrl.h>

#include <algorithm>
#include <string>
#include <utility>

#include "ordo/gui/task_dialog.h"

namespace ordo::gui {
namespace {

constexpr int kWindowWidth = 1120;
constexpr int kWindowHeight = 720;
constexpr int kMinWidth = 900;
constexpr int kMinHeight = 600;

constexpr COLORREF kBackground = RGB(247, 248, 250);
constexpr COLORREF kTitleColor = RGB(27, 37, 59);
constexpr COLORREF kTextColor = RGB(55, 65, 81);
constexpr COLORREF kMutedColor = RGB(107, 114, 128);

enum ControlId : int {
    kList = 2001,
    kAddButton = 2002,
    kEditButton = 2003,
    kDoneButton = 2004,
    kRemoveButton = 2005,
    kStateFilter = 2006,
    kRankFilter = 2007,
    kRefreshButton = 2008,
    kPlanText = 2009,
    kStatsText = 2010,
    kStatus = 2011,
    kTitle = 2012,
    kSubtitle = 2013,
    kPlanHeading = 2014,
    kStatsHeading = 2015,
};

const wchar_t* kClassName = L"OrdoMainWindow";

std::wstring state_text(TaskState state) {
    switch (state) {
        case TaskState::Todo:
            return L"待办";
        case TaskState::Doing:
            return L"进行中";
        case TaskState::Done:
            return L"已完成";
        case TaskState::Dropped:
            return L"已放弃";
    }
    return L"待办";
}

void show_error(HWND parent, const Status& status) {
    MessageBoxW(parent, to_wide(status.message).c_str(), L"Ordo", MB_OK | MB_ICONERROR);
}

}  // namespace

MainWindow::MainWindow(ITaskRepository& repo)
    : background_(CreateSolidBrush(kBackground)),
      model_(repo, [] { return Clock::now(); }) {}

MainWindow::~MainWindow() {
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
    if (title_font_ != nullptr) {
        DeleteObject(title_font_);
    }
    if (heading_font_ != nullptr) {
        DeleteObject(heading_font_);
    }
    if (small_font_ != nullptr) {
        DeleteObject(small_font_);
    }
    if (background_ != nullptr) {
        DeleteObject(background_);
    }
}

bool MainWindow::create(HINSTANCE instance, int show_command) {
    instance_ = instance;
    dpi_ = system_dpi();

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kClassName;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = background_;
    window_class.style = CS_HREDRAW | CS_VREDRAW;

    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    RECT frame{0, 0, scale_for_dpi(dpi_, kWindowWidth), scale_for_dpi(dpi_, kWindowHeight)};
    AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);

    hwnd_ = CreateWindowExW(0, kClassName, L"Ordo · 学习任务编排器", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                            frame.bottom - frame.top, nullptr, nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return false;
    }

    create_children();
    layout();
    refresh();

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    return true;
}

int MainWindow::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK MainWindow::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        auto* self = static_cast<MainWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->handle(message, wparam, lparam);
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

LRESULT MainWindow::handle(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_SIZE:
            layout();
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
            info->ptMinTrackSize.x = scale_for_dpi(dpi_, kMinWidth);
            info->ptMinTrackSize.y = scale_for_dpi(dpi_, kMinHeight);
            return 0;
        }

        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);

            if (code == CBN_SELCHANGE && (id == kStateFilter || id == kRankFilter)) {
                refresh();
                return 0;
            }

            if (code == BN_CLICKED) {
                switch (id) {
                    case kAddButton:
                        on_add();
                        return 0;
                    case kEditButton:
                        on_edit();
                        return 0;
                    case kDoneButton:
                        on_done();
                        return 0;
                    case kRemoveButton:
                        on_remove();
                        return 0;
                    case kRefreshButton:
                        refresh();
                        return 0;
                    default:
                        break;
                }
            }
            break;
        }

        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(lparam);
            if (header->idFrom == kList) {
                if (header->code == NM_DBLCLK) {
                    on_edit();
                    return 0;
                }
                if (header->code == LVN_ITEMCHANGED) {
                    update_buttons();
                    return 0;
                }
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(dc, TRANSPARENT);
            const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lparam));
            if (id == kTitle) {
                SetTextColor(dc, kTitleColor);
            } else if (id == kSubtitle || id == kStatus) {
                SetTextColor(dc, kMutedColor);
            } else {
                SetTextColor(dc, kTextColor);
            }
            return reinterpret_cast<LRESULT>(background_);
        }

        case WM_CLOSE:
            DestroyWindow(hwnd_);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return DefWindowProcW(hwnd_, message, wparam, lparam);
}

void MainWindow::create_children() {
    font_ = create_ui_font(dpi_, 10);
    title_font_ = create_ui_font(dpi_, 20, FW_BOLD);
    heading_font_ = create_ui_font(dpi_, 11, FW_SEMIBOLD);
    small_font_ = create_ui_font(dpi_, 9);

    auto add_text = [this](int id, const wchar_t* text, DWORD style, HFONT font) {
        HWND control = CreateWindowExW(
            0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, hwnd_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        return control;
    };

    auto add_button = [this](int id, const wchar_t* text) {
        HWND control = CreateWindowExW(
            0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, 0, 0, 0,
            hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
        return control;
    };

    add_text(kTitle, L"Ordo", SS_LEFT, title_font_);
    add_text(kSubtitle, L"学习任务编排器 · 先做重要的，再做紧急的", SS_LEFT, small_font_);

    add_button_ = add_button(kAddButton, L"添加任务");
    edit_button_ = add_button(kEditButton, L"编辑");
    done_button_ = add_button(kDoneButton, L"标记完成");
    remove_button_ = add_button(kRemoveButton, L"删除");
    refresh_button_ = add_button(kRefreshButton, L"刷新");

    state_label_ = add_text(0, L"状态", SS_LEFT, font_);
    state_combo_ = CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kStateFilter)),
        instance_, nullptr);
    SendMessageW(state_combo_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"未完成"));
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"全部"));
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"待办"));
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"进行中"));
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"已完成"));
    SendMessageW(state_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"已放弃"));
    SendMessageW(state_combo_, CB_SETCURSEL, 0, 0);

    rank_label_ = add_text(0, L"排序", SS_LEFT, font_);
    rank_combo_ = CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kRankFilter)),
        instance_, nullptr);
    SendMessageW(rank_combo_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    SendMessageW(rank_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"默认顺序"));
    SendMessageW(rank_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"加权优先级"));
    SendMessageW(rank_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"最早截止"));
    SendMessageW(rank_combo_, CB_SETCURSEL, 0, 0);

    list_ = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                                LVS_SHOWSELALWAYS,
                            0, 0, 0, 0, hwnd_,
                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kList)), instance_,
                            nullptr);
    SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(font_), TRUE);
    ListView_SetExtendedListViewStyle(
        list_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    struct ColumnSpec {
        const wchar_t* text;
        int width;
        int format;
    };
    const ColumnSpec columns[] = {
        {L"ID", 56, LVCFMT_LEFT},        {L"标题", 240, LVCFMT_LEFT},
        {L"课程", 110, LVCFMT_LEFT},     {L"重要度", 70, LVCFMT_CENTER},
        {L"预估(分)", 80, LVCFMT_RIGHT}, {L"截止时间", 140, LVCFMT_LEFT},
        {L"状态", 80, LVCFMT_LEFT},
    };
    for (int i = 0; i < static_cast<int>(sizeof(columns) / sizeof(columns[0])); ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
        column.pszText = const_cast<wchar_t*>(columns[i].text);
        column.cx = scale_for_dpi(dpi_, columns[i].width);
        column.fmt = columns[i].format;
        column.iSubItem = i;
        ListView_InsertColumn(list_, i, &column);
    }

    add_text(kPlanHeading, L"今日计划", SS_LEFT, heading_font_);
    plan_text_ = add_text(kPlanText,
                          L"尚未生成。\n\n"
                          L"M2 编排算法接入后，\n"
                          L"这里将显示时间块、\n"
                          L"未排上的任务与冲突告警。",
                          SS_LEFT, font_);

    add_text(kStatsHeading, L"统计", SS_LEFT, heading_font_);
    stats_text_ = add_text(kStatsText, L"", SS_LEFT, font_);

    status_ = add_text(kStatus, L"就绪", SS_LEFT, small_font_);
}

void MainWindow::layout() {
    if (list_ == nullptr) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd_, &client);
    const auto s = [this](int value) { return scale_for_dpi(dpi_, value); };

    const int pad = s(16);
    const int button_height = s(30);
    const int content_top = s(124);
    const int status_height = s(24);
    const int content_bottom = static_cast<int>(client.bottom) - status_height - s(10);
    const int content_height = std::max(s(120), content_bottom - content_top);
    const int panel_width = s(300);
    const int list_width = std::max(
        s(320), static_cast<int>(client.right) - pad * 2 - panel_width - pad);

    // 标题区
    MoveWindow(GetDlgItem(hwnd_, kTitle), pad, s(10), s(500), s(34), TRUE);
    MoveWindow(GetDlgItem(hwnd_, kSubtitle), pad, s(46), s(600), s(20), TRUE);

    // 工具栏
    const int toolbar_y = s(78);
    int x = pad;
    const int gap = s(8);
    MoveWindow(add_button_, x, toolbar_y, s(88), button_height, TRUE);
    x += s(88) + gap;
    MoveWindow(edit_button_, x, toolbar_y, s(72), button_height, TRUE);
    x += s(72) + gap;
    MoveWindow(done_button_, x, toolbar_y, s(88), button_height, TRUE);
    x += s(88) + gap;
    MoveWindow(remove_button_, x, toolbar_y, s(72), button_height, TRUE);
    x += s(72) + s(24);

    MoveWindow(state_label_, x, toolbar_y + s(6), s(36), s(20), TRUE);
    x += s(36);
    MoveWindow(state_combo_, x, toolbar_y, s(110), s(200), TRUE);
    x += s(110) + s(16);
    MoveWindow(rank_label_, x, toolbar_y + s(6), s(36), s(20), TRUE);
    x += s(36);
    MoveWindow(rank_combo_, x, toolbar_y, s(140), s(200), TRUE);
    x += s(140) + s(16);
    MoveWindow(refresh_button_, x, toolbar_y, s(64), button_height, TRUE);

    // 列表
    MoveWindow(list_, pad, content_top, list_width, content_height, TRUE);
    const int fixed_width = s(56 + 110 + 70 + 80 + 140 + 80);
    ListView_SetColumnWidth(list_, 1, std::max(s(180), list_width - fixed_width - s(24)));

    // 右侧面板
    const int panel_x = pad + list_width + pad;
    MoveWindow(GetDlgItem(hwnd_, kPlanHeading), panel_x, content_top, panel_width, s(24), TRUE);
    MoveWindow(plan_text_, panel_x, content_top + s(32), panel_width, s(150), TRUE);

    const int stats_top = content_top + s(198);
    MoveWindow(GetDlgItem(hwnd_, kStatsHeading), panel_x, stats_top, panel_width, s(24), TRUE);
    MoveWindow(stats_text_, panel_x, stats_top + s(30), panel_width,
               std::max(s(80), content_bottom - stats_top - s(30)), TRUE);

    MoveWindow(status_, pad, client.bottom - status_height - s(6), client.right - pad * 2,
               status_height, TRUE);
}

void MainWindow::refresh() {
    if (list_ == nullptr) {
        return;
    }

    auto rows = model_.list(current_query());

    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(list_);

    if (!rows.has_value()) {
        SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_, nullptr, TRUE);
        set_status(L"读取任务失败：" + to_wide(rows.status().message));
        update_stats();
        update_buttons();
        return;
    }

    int index = 0;
    for (const TaskRow& row : rows.value()) {
        const std::wstring id_text = std::to_wstring(row.id);
        const std::wstring title = to_wide(row.title);
        const std::wstring course = to_wide(row.course);
        const std::wstring importance = std::to_wstring(row.importance);
        const std::wstring estimate = std::to_wstring(row.estimate);
        const std::wstring deadline = to_wide(row.deadline_text);
        const std::wstring state = state_text(row.state);

        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = index;
        item.lParam = static_cast<LPARAM>(row.id);
        item.pszText = const_cast<wchar_t*>(id_text.c_str());
        ListView_InsertItem(list_, &item);

        ListView_SetItemText(list_, index, 1, const_cast<wchar_t*>(title.c_str()));
        ListView_SetItemText(list_, index, 2, const_cast<wchar_t*>(course.c_str()));
        ListView_SetItemText(list_, index, 3, const_cast<wchar_t*>(importance.c_str()));
        ListView_SetItemText(list_, index, 4, const_cast<wchar_t*>(estimate.c_str()));
        ListView_SetItemText(list_, index, 5, const_cast<wchar_t*>(deadline.c_str()));
        ListView_SetItemText(list_, index, 6, const_cast<wchar_t*>(state.c_str()));
        ++index;
    }

    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);
    set_status(L"共 " + std::to_wstring(rows.value().size()) + L" 条任务");

    update_stats();
    update_buttons();
}

void MainWindow::update_stats() {
    auto stats = model_.stats();
    if (!stats.has_value()) {
        SetWindowTextW(stats_text_, L"统计失败");
        return;
    }

    const Stats& value = stats.value();
    std::wstring text;
    text += L"总数　　　　" + std::to_wstring(value.total) + L"\n";
    text += L"未完成　　　" + std::to_wstring(value.todo + value.doing) + L"\n";
    text += L"进行中　　　" + std::to_wstring(value.doing) + L"\n";
    text += L"已完成　　　" + std::to_wstring(value.done) + L"\n";
    text += L"超期　　　　" + std::to_wstring(value.overdue) + L"\n";
    text += L"预估总时长　" + std::to_wstring(value.open_estimate) + L" 分钟";
    SetWindowTextW(stats_text_, text.c_str());
}

void MainWindow::update_buttons() {
    const bool has_selection = selected_id() != 0;
    EnableWindow(edit_button_, has_selection);
    EnableWindow(done_button_, has_selection);
    EnableWindow(remove_button_, has_selection);
}

void MainWindow::set_status(const std::wstring& text) {
    SetWindowTextW(status_, text.c_str());
}

ListQuery MainWindow::current_query() const {
    ListQuery query;

    const int state_index =
        static_cast<int>(SendMessageW(state_combo_, CB_GETCURSEL, 0, 0));
    switch (state_index) {
        case 1:
            query.open_only = false;
            break;
        case 2:
            query.state = TaskState::Todo;
            break;
        case 3:
            query.state = TaskState::Doing;
            break;
        case 4:
            query.state = TaskState::Done;
            break;
        case 5:
            query.state = TaskState::Dropped;
            break;
        default:
            query.state.reset();
            query.open_only = true;
            break;
    }

    const int rank_index = static_cast<int>(SendMessageW(rank_combo_, CB_GETCURSEL, 0, 0));
    if (rank_index == 1) {
        query.rank = RankMode::Score;
    } else if (rank_index == 2) {
        query.rank = RankMode::Edf;
    } else {
        query.rank = RankMode::None;
    }

    return query;
}

TaskId MainWindow::selected_id() const {
    const int index = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    if (index < 0) {
        return 0;
    }

    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(list_, &item) == FALSE) {
        return 0;
    }
    return static_cast<TaskId>(item.lParam);
}

void MainWindow::on_add() {
    if (show_task_dialog(hwnd_, dpi_, model_, std::nullopt)) {
        refresh();
        set_status(L"任务已添加");
    }
}

void MainWindow::on_edit() {
    const TaskId id = selected_id();
    if (id == 0) {
        return;
    }
    if (show_task_dialog(hwnd_, dpi_, model_, id)) {
        refresh();
        set_status(L"任务 #" + std::to_wstring(id) + L" 已更新");
    }
}

void MainWindow::on_done() {
    const TaskId id = selected_id();
    if (id == 0) {
        return;
    }

    const Status status = model_.set_state(id, TaskState::Done);
    if (!status.ok()) {
        show_error(hwnd_, status);
        return;
    }

    refresh();
    set_status(L"任务 #" + std::to_wstring(id) + L" 已标记完成");
}

void MainWindow::on_remove() {
    const TaskId id = selected_id();
    if (id == 0) {
        return;
    }

    const int answer = MessageBoxW(hwnd_, L"删除后无法恢复，确定删除这个任务吗？", L"确认删除",
                                   MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer != IDYES) {
        return;
    }

    const Status status = model_.remove(id);
    if (!status.ok()) {
        show_error(hwnd_, status);
        return;
    }

    refresh();
    set_status(L"任务 #" + std::to_wstring(id) + L" 已删除");
}

}  // namespace ordo::gui
