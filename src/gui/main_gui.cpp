// Ordo GUI 入口：装配仓库与主窗口。
// 与 src/main.cpp 一样，这里也是唯一的依赖装配点（GUI 侧）。

#include "ordo/gui/main_window.h"
#include "ordo/gui/win32_util.h"
#include "ordo/io/config.h"
#include "ordo/io/event_log.h"
#include "ordo/io/file_repository.h"

#include <commctrl.h>
#include <shellapi.h>

#include <exception>
#include <filesystem>
#include <string>
#include <string_view>

namespace {

std::filesystem::path executable_path() {
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(std::wstring{buffer, length});
}

// GUI 允许双击启动，因此数据路径不能依赖当前工作目录：
// 优先命令行 --data，其次 exe 同级的 ../data（仓库布局），最后 exe 同级的 data。
std::filesystem::path resolve_data_path() {
    int argc = 0;
    LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (arguments != nullptr) {
        std::filesystem::path explicit_path;
        for (int i = 1; i < argc; ++i) {
            const std::wstring_view argument{arguments[i]};
            if (argument == L"--data" && i + 1 < argc) {
                explicit_path = arguments[++i];
                break;
            }
            constexpr std::wstring_view kPrefix = L"--data=";
            if (argument.rfind(kPrefix, 0) == 0) {
                explicit_path = std::wstring{argument.substr(kPrefix.size())};
                break;
            }
        }
        LocalFree(arguments);
        if (!explicit_path.empty()) {
            return explicit_path;
        }
    }

    const std::filesystem::path exe = executable_path();
    if (!exe.empty()) {
        const std::filesystem::path repo_data = exe.parent_path().parent_path() / "data";
        std::error_code error;
        if (std::filesystem::exists(repo_data, error)) {
            return repo_data / "tasks.csv";
        }
        return exe.parent_path() / "data" / "tasks.csv";
    }

    return std::filesystem::path(L"data") / "tasks.csv";
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE /*previous*/, PWSTR /*command_line*/,
                    int show_command) {
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&controls);

    try {
        const std::filesystem::path data_path = resolve_data_path();
        ordo::CsvTaskRepository repo(data_path);
        ordo::CsvEventLog event_log(data_path.parent_path() / "log.csv");

        const auto config = ordo::load_config(data_path.parent_path() / "config.ini");
        if (!config.has_value()) {
            MessageBoxW(nullptr, ordo::gui::to_wide(config.status().message).c_str(), L"Ordo",
                        MB_OK | MB_ICONERROR);
            return 1;
        }

        ordo::gui::MainWindow window(repo, config.value(), &event_log);

        if (!window.create(instance, show_command)) {
            MessageBoxW(nullptr, L"主窗口创建失败。", L"Ordo", MB_OK | MB_ICONERROR);
            return 1;
        }
        return window.run();
    } catch (const std::exception& error) {
        MessageBoxW(nullptr, ordo::gui::to_wide(error.what()).c_str(), L"Ordo",
                    MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        MessageBoxW(nullptr, L"发生未知错误。", L"Ordo", MB_OK | MB_ICONERROR);
        return 1;
    }
}
