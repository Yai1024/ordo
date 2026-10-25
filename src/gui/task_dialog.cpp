#include "ordo/gui/task_dialog.h"

#include <commctrl.h>

#include <cstddef>
#include <ctime>
#include <cwchar>
#include <string>
#include <utility>

#include "ordo/core/types.h"

namespace ordo::gui {
namespace {

constexpr int kClientWidth = 460;
constexpr int kClientHeight = 372;
constexpr int kMargin = 18;
constexpr int kLabelWidth = 90;
constexpr int kInputX = 118;
constexpr int kInputWidth = 318;

enum ControlId : int {
    kTitleEdit = 1001,
    kCourseEdit = 1002,
    kImportanceCombo = 1003,
    kEstimateEdit = 1004,
    kDeadlinePicker = 1005,
    kStateCombo = 1006,
    kSaveButton = 1007,
    kCancelButton = 1008,
    kErrorLabel = 1009,
    kStateLabel = 1010,
    kKindCombo = 1011,
    kDeadlineLabel = 1012,
};

struct DialogState {
    HWND hwnd{};
    HWND title_edit{};
    HWND course_edit{};
    HWND kind_combo{};
    HWND importance_combo{};
    HWND estimate_edit{};
    HWND deadline_picker{};
    HWND deadline_label{};
    HWND state_combo{};
    HWND state_label{};
    HWND error_label{};
    HFONT font{};
    HFONT bold_font{};
    AppModel* model{};
    std::optional<TaskId> edit_id;
    TaskForm initial;
    int dpi{96};
    bool saved{false};
};

std::wstring window_text(HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

SYSTEMTIME system_time_from_text(const std::string& text) {
    SYSTEMTIME result{};
    const std::optional<TimePoint> parsed = parse_datetime(text);
    if (!parsed.has_value()) {
        GetLocalTime(&result);
        return result;
    }
    const std::time_t seconds = Clock::to_time_t(*parsed);
    std::tm parts{};
    if (const std::tm* local = std::localtime(&seconds)) {
        parts = *local;
    }
    result.wYear = static_cast<WORD>(parts.tm_year + 1900);
    result.wMonth = static_cast<WORD>(parts.tm_mon + 1);
    result.wDay = static_cast<WORD>(parts.tm_mday);
    result.wHour = static_cast<WORD>(parts.tm_hour);
    result.wMinute = static_cast<WORD>(parts.tm_min);
    return result;
}

SYSTEMTIME default_deadline() {
    const std::time_t tomorrow = std::time(nullptr) + 24 * 60 * 60;
    std::tm parts{};
    if (const std::tm* local = std::localtime(&tomorrow)) {
        parts = *local;
    }
    SYSTEMTIME result{};
    result.wYear = static_cast<WORD>(parts.tm_year + 1900);
    result.wMonth = static_cast<WORD>(parts.tm_mon + 1);
    result.wDay = static_cast<WORD>(parts.tm_mday);
    result.wHour = 23;
    result.wMinute = 59;
    return result;
}

std::wstring format_system_time(const SYSTEMTIME& time) {
    wchar_t buffer[32] = {};
    std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%04u-%02u-%02u %02u:%02u",
                  time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    return buffer;
}

HWND create_control(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w,
                    int h, HWND parent, int id, HFONT font) {
    HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h,
                                   parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
    if (control != nullptr && font != nullptr) {
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    return control;
}

void select_combo_text(HWND combo, int index) {
    SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

int kind_index_from_text(const std::string& text) {
    const std::string value = text.empty() ? std::string{"other"} : text;
    const std::optional<TaskKind> kind = parse_task_kind(value);
    return kind.has_value() ? static_cast<int>(*kind) : static_cast<int>(TaskKind::Other);
}

// 活动把同一控件从"截止时间"解释为"开始时间"，避免再加一个日期选择器。
void update_kind_ui(DialogState& state) {
    const int index = static_cast<int>(SendMessageW(state.kind_combo, CB_GETCURSEL, 0, 0));
    const bool activity = index == static_cast<int>(TaskKind::Activity);
    SetWindowTextW(state.deadline_label, activity ? L"开始时间" : L"截止时间");
}

void create_controls(DialogState& state) {
    const auto s = [dpi = state.dpi](int value) { return scale_for_dpi(dpi, value); };
    const int row = s(36);
    const int first_row = s(18);
    const int input_height = s(24);

    state.font = create_ui_font(state.dpi, 10);
    state.bold_font = create_ui_font(state.dpi, 10, FW_SEMIBOLD);

    auto label = [&](int id, const wchar_t* text, int y) {
        return create_control(L"STATIC", text, SS_LEFT, s(kMargin), y, s(kLabelWidth), s(20),
                              state.hwnd, id, state.font);
    };

    label(-1, L"标题", first_row);
    label(-1, L"课程", first_row + row);
    label(-1, L"类型", first_row + row * 2);
    label(-1, L"重要度", first_row + row * 3);
    label(-1, L"预估（分钟）", first_row + row * 4);
    state.deadline_label = label(kDeadlineLabel, L"截止时间", first_row + row * 5);
    state.state_label = label(kStateLabel, L"状态", first_row + row * 6);

    state.title_edit =
        create_control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, s(kInputX),
                       first_row, s(kInputWidth), input_height, state.hwnd, kTitleEdit,
                       state.font);
    state.course_edit =
        create_control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER, s(kInputX),
                       first_row + row, s(kInputWidth), input_height, state.hwnd, kCourseEdit,
                       state.font);

    state.kind_combo =
        create_control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, s(kInputX),
                       first_row + row * 2, s(150), s(180), state.hwnd, kKindCombo, state.font);
    SendMessageW(state.kind_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"课程作业"));
    SendMessageW(state.kind_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"自主学习"));
    SendMessageW(state.kind_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"活动"));
    SendMessageW(state.kind_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"复习考试"));
    SendMessageW(state.kind_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"其他"));

    state.importance_combo =
        create_control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, s(kInputX),
                       first_row + row * 3, s(90), s(160), state.hwnd, kImportanceCombo,
                       state.font);
    for (int value = 1; value <= 5; ++value) {
        const std::wstring item = std::to_wstring(value);
        SendMessageW(state.importance_combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(item.c_str()));
    }

    state.estimate_edit =
        create_control(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER | WS_BORDER,
                       s(kInputX), first_row + row * 4, s(kInputWidth), input_height,
                       state.hwnd, kEstimateEdit, state.font);

    state.deadline_picker =
        create_control(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, s(kInputX),
                       first_row + row * 5, s(kInputWidth), input_height, state.hwnd,
                       kDeadlinePicker, state.font);
    DateTime_SetFormat(state.deadline_picker, L"yyyy-MM-dd HH:mm");

    state.state_combo =
        create_control(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, s(kInputX),
                       first_row + row * 6, s(140), s(160), state.hwnd, kStateCombo,
                       state.font);
    SendMessageW(state.state_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"待办"));
    SendMessageW(state.state_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"进行中"));
    SendMessageW(state.state_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"已完成"));
    SendMessageW(state.state_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"已放弃"));

    state.error_label =
        create_control(L"STATIC", L"", SS_LEFT, s(kMargin), first_row + row * 7,
                       s(kClientWidth - kMargin * 2), s(42), state.hwnd, kErrorLabel,
                       state.font);

    const int button_width = s(84);
    const int button_height = s(30);
    const int cancel_x = s(kClientWidth - kMargin - 84);
    const int save_x = cancel_x - s(8) - button_width;
    CreateWindowExW(0, L"BUTTON", L"取消",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, cancel_x, s(328),
                    button_width, button_height, state.hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kCancelButton)),
                    GetModuleHandleW(nullptr), nullptr);
    CreateWindowExW(0, L"BUTTON", L"保存",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, save_x, s(328),
                    button_width, button_height, state.hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kSaveButton)),
                    GetModuleHandleW(nullptr), nullptr);
    SendMessageW(GetDlgItem(state.hwnd, kCancelButton), WM_SETFONT,
                 reinterpret_cast<WPARAM>(state.font), TRUE);
    SendMessageW(GetDlgItem(state.hwnd, kSaveButton), WM_SETFONT,
                 reinterpret_cast<WPARAM>(state.font), TRUE);

    // 初始值
    SetWindowTextW(state.title_edit, to_wide(state.initial.title).c_str());
    SetWindowTextW(state.course_edit, to_wide(state.initial.course).c_str());
    SetWindowTextW(state.estimate_edit, to_wide(state.initial.estimate).c_str());

    const int importance = std::stoi(state.initial.importance.empty() ? "3"
                                                                      : state.initial.importance);
    select_combo_text(state.importance_combo, importance >= 1 && importance <= 5 ? importance - 1
                                                                                 : 2);

    const int kind_index = kind_index_from_text(state.initial.kind);
    select_combo_text(state.kind_combo, kind_index);

    if (state.edit_id.has_value()) {
        const bool activity = kind_index == static_cast<int>(TaskKind::Activity);
        const std::string picker_text =
            activity && !state.initial.start.empty() ? state.initial.start : state.initial.deadline;
        const SYSTEMTIME deadline = system_time_from_text(picker_text);
        DateTime_SetSystemtime(state.deadline_picker, GDT_VALID, &deadline);
        const int state_index =
            static_cast<int>(state.initial.state.value_or(TaskState::Todo));
        select_combo_text(state.state_combo, state_index);
    } else {
        const SYSTEMTIME deadline = default_deadline();
        DateTime_SetSystemtime(state.deadline_picker, GDT_VALID, &deadline);
        select_combo_text(state.state_combo, 0);
        ShowWindow(state.state_label, SW_HIDE);
        ShowWindow(state.state_combo, SW_HIDE);
    }

    update_kind_ui(state);
}

void save_dialog(DialogState& state) {
    TaskForm form;
    form.title = to_utf8(window_text(state.title_edit));
    form.course = to_utf8(window_text(state.course_edit));
    form.estimate = to_utf8(window_text(state.estimate_edit));

    int importance = static_cast<int>(SendMessageW(state.importance_combo, CB_GETCURSEL, 0, 0));
    if (importance < 0) {
        importance = 2;
    }
    form.importance = std::to_string(importance + 1);

    int kind_index = static_cast<int>(SendMessageW(state.kind_combo, CB_GETCURSEL, 0, 0));
    if (kind_index < 0) {
        kind_index = static_cast<int>(TaskKind::Other);
    }
    const TaskKind kind = static_cast<TaskKind>(kind_index);
    form.kind = std::string{to_string(kind)};

    SYSTEMTIME picker{};
    DateTime_GetSystemtime(state.deadline_picker, &picker);
    const std::string picker_text = to_utf8(format_system_time(picker));
    if (is_fixed_time(kind)) {
        // 活动把这个控件当"开始时间"用；截止时间由 AppModel 按开始 + 预估补全。
        form.start = picker_text;
        form.deadline.clear();
    } else {
        form.deadline = picker_text;
    }

    if (state.edit_id.has_value()) {
        int index = static_cast<int>(SendMessageW(state.state_combo, CB_GETCURSEL, 0, 0));
        if (index < 0) {
            index = 0;
        }
        form.state = static_cast<TaskState>(index);
    }

    Status result;
    if (state.edit_id.has_value()) {
        result = state.model->update(*state.edit_id, form);
    } else {
        auto created = state.model->create(form);
        result = created.has_value()
                     ? Status::success()
                     : Status::fail(created.status().code, created.status().message);
    }

    if (!result.ok()) {
        SetWindowTextW(state.error_label, to_wide(result.message).c_str());
        return;
    }

    state.saved = true;
    DestroyWindow(state.hwnd);
}

LRESULT CALLBACK dialog_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_CREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            state = static_cast<DialogState*>(create->lpCreateParams);
            state->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            create_controls(*state);
            return 0;
        }
        case WM_COMMAND: {
            if (state == nullptr) {
                break;
            }
            const int id = LOWORD(wparam);
            if (id == kKindCombo && HIWORD(wparam) == CBN_SELCHANGE) {
                update_kind_ui(*state);
                return 0;
            }
            if (id == kSaveButton && HIWORD(wparam) == BN_CLICKED) {
                save_dialog(*state);
                return 0;
            }
            if (id == kCancelButton || id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetBkMode(dc, TRANSPARENT);
            const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lparam));
            SetTextColor(dc, id == kErrorLabel ? RGB(185, 28, 28) : RGB(55, 65, 81));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        case WM_DESTROY:
            if (state != nullptr) {
                if (state->font != nullptr) {
                    DeleteObject(state->font);
                    state->font = nullptr;
                }
                if (state->bold_font != nullptr) {
                    DeleteObject(state->bold_font);
                    state->bold_font = nullptr;
                }
            }
            return 0;
        default:
            break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool ensure_dialog_class() {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = dialog_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.lpszClassName = L"OrdoTaskDialog";
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);

    const ATOM atom = RegisterClassExW(&window_class);
    registered = atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    return registered;
}

}  // namespace

bool show_task_dialog(HWND parent, int dpi, AppModel& model, std::optional<TaskId> edit_id) {
    if (!ensure_dialog_class()) {
        return false;
    }

    DialogState state;
    state.model = &model;
    state.edit_id = edit_id;
    state.dpi = dpi;

    if (edit_id.has_value()) {
        auto form = model.form_for(*edit_id);
        if (!form.has_value()) {
            MessageBoxW(parent, to_wide(form.status().message).c_str(), L"Ordo",
                        MB_OK | MB_ICONERROR);
            return false;
        }
        state.initial = form.value();
    } else {
        state.initial.title.clear();
        state.initial.course.clear();
        state.initial.kind = "other";
        state.initial.importance = "3";
        state.initial.estimate = "60";
        state.initial.deadline.clear();
        state.initial.start.clear();
    }

    RECT parent_rect{};
    GetWindowRect(parent, &parent_rect);

    // 控件坐标按客户区计算，窗口尺寸要补上标题栏与边框。
    RECT frame{0, 0, scale_for_dpi(dpi, kClientWidth), scale_for_dpi(dpi, kClientHeight)};
    AdjustWindowRectEx(&frame, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT);

    const int width = frame.right - frame.left;
    const int height = frame.bottom - frame.top;
    const int x = parent_rect.left + ((parent_rect.right - parent_rect.left) - width) / 2;
    const int y = parent_rect.top + ((parent_rect.bottom - parent_rect.top) - height) / 2;

    // WS_EX_CONTROLPARENT 让 IsDialogMessageW 在自定义窗口里也能做 Tab 导航。
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, L"OrdoTaskDialog",
        edit_id.has_value() ? L"编辑任务" : L"添加任务", WS_POPUP | WS_CAPTION | WS_SYSMENU, x,
        y, width, height, parent, nullptr, GetModuleHandleW(nullptr), &state);
    if (dialog == nullptr) {
        return false;
    }

    EnableWindow(parent, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
    SetFocus(state.title_edit);

    MSG message{};
    while (IsWindow(dialog)) {
        const BOOL received = GetMessageW(&message, nullptr, 0, 0);
        if (received <= 0) {
            break;
        }

        // 自定义窗口没有对话框模板，IsDialogMessage 不保证处理这三个键，
        // 因此显式实现 Tab 导航、回车默认按钮与 Esc 取消。
        if (message.message == WM_KEYDOWN &&
            (message.hwnd == dialog || IsChild(dialog, message.hwnd))) {
            if (message.wParam == VK_TAB) {
                const bool backwards = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                HWND next = GetNextDlgTabItem(dialog, GetFocus(), backwards);
                if (next != nullptr) {
                    SetFocus(next);
                }
                continue;
            }
            if (message.wParam == VK_RETURN) {
                const int focused_id = GetDlgCtrlID(GetFocus());
                if (focused_id == kCancelButton) {
                    DestroyWindow(dialog);
                } else {
                    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(kSaveButton, BN_CLICKED), 0);
                }
                continue;
            }
            if (message.wParam == VK_ESCAPE) {
                DestroyWindow(dialog);
                continue;
            }
        }

        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (IsWindow(dialog)) {
        DestroyWindow(dialog);
    }
    EnableWindow(parent, TRUE);
    SetActiveWindow(parent);
    return state.saved;
}

}  // namespace ordo::gui
